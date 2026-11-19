' FILE: Browser/BrowserAuthNetTest.bas
' D3 del piano automazione — autenticazione/rete via CDP Network.*:
'   - BrowserSetFetchHeaders (Fetch.enable + continueRequest): applica gli header custom in
'     modo AFFIDABILE (Network.setExtraHTTPHeaders non li applica su Chrome 149 headless).
'   - BrowserSetCookie (Network.setCookie), BrowserSetUserAgent (Network.setUserAgentOverride).
' L'echo HTTP (qui: un server esterno che risponde con gli header ricevuti) mostra se
' cookie/UA/header raggiungono davvero il server.
'
' PREREQUISITI: Chromium headless su :9222 (plugins/browser/launch_chromium.sh) + un echo
' server su :8098 che risponde con 'X-Custom|User-Agent|Cookie' (es. `python3 -m http.server`
' custom). Nel gate/CI si usa un echo locale.

IMPORT NET
IMPORT HTTP
IMPORT STRINGS
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY

DIM cDpResult AS STRING
DIM cDpReady AS BIT
DIM cDpFetchEnabled AS INTEGER
DIM cDpFetchHeaders AS STRING

SUB CdpHandler(ws AS WEBSOCKET, msg$)
  cDpResult = msg$
  cDpReady = 1
END SUB

SUB CdpFetchHandler(ws AS WEBSOCKET, msg$)
  DIM rid AS STRING
  DIM p1 AS INTEGER
  DIM needle AS STRING
  IF INSTR(msg$, CHR$(34) + "id" + CHR$(34) + ":30") > 0 THEN
    cDpResult = msg$
    cDpReady = 1
    EXIT SUB
  END IF
  IF cDpFetchEnabled AND INSTR(msg$, "requestPaused") > 0 THEN
    needle = CHR$(34) + "requestId" + CHR$(34) + ":" + CHR$(34)
    p1 = INSTR(msg$, needle)
    IF p1 > 0 THEN
      p1 = p1 + LEN(needle)
      rid = MID$(msg$, p1, INSTR(MID$(msg$, p1, LEN(msg$) - p1 + 1), CHR$(34)) - 1)
      ws.Send("{""id"":40,""method"":""Fetch.continueRequest"",""params"":{""requestId"":""" + rid + CHR$(34) + ",""headers"":" + cDpFetchHeaders + "}}")
    END IF
  END IF
END SUB

FUNCTION CdpPath(port AS INTEGER) AS STRING
  DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:" + LTRIM(STR$(port)) + "/json")
  DIM j$ AS STRING
  DIM prefix$ AS STRING
  DIM p1 AS INTEGER
  DIM p2 AS INTEGER
  j$ = r.Body
  prefix$ = "ws://127.0.0.1:" + LTRIM(STR$(port))
  p1 = INSTR(j$, prefix$)
  IF p1 = 0 THEN
    CdpPath = "/devtools/page/1"
  ELSE
    p1 = p1 + LEN(prefix$)
    p2 = INSTR(MID$(j$, p1, LEN(j$) - p1 + 1), CHR$(34))
    IF p2 = 0 THEN
      CdpPath = MID$(j$, p1, LEN(j$) - p1 + 1)
    ELSE
      CdpPath = MID$(j$, p1, p2 - 1)
    END IF
  END IF
END FUNCTION

FUNCTION CdpJsonStringValue(raw AS STRING, key AS STRING) AS STRING
  DIM needle AS STRING
  DIM p1 AS INTEGER
  DIM p2 AS INTEGER
  needle = CHR$(34) + key + CHR$(34) + ":" + CHR$(34)
  p1 = INSTR(raw, needle)
  IF p1 = 0 THEN
    CdpJsonStringValue = ""
  ELSE
    p1 = p1 + LEN(needle)
    p2 = INSTR(MID$(raw, p1, LEN(raw) - p1 + 1), CHR$(34))
    IF p2 = 0 THEN
      CdpJsonStringValue = MID$(raw, p1, LEN(raw) - p1 + 1)
    ELSE
      CdpJsonStringValue = MID$(raw, p1, p2 - 1)
    END IF
  END IF
END FUNCTION

FUNCTION CdpCommand(cmd AS STRING, port AS INTEGER) AS STRING
  DIM ws AS WEBSOCKET
  DIM guard AS INTEGER
  WEBSOCKET CONNECT "127.0.0.1", port, ws, CdpPath(port)
  IF ws IS NOTHING THEN
    CdpCommand = "ERROR: CDP non raggiungibile"
    EXIT FUNCTION
  END IF
  ws.OnMessage = "CdpHandler"
  cDpReady = 0
  ws.Send(cmd)
  guard = 0
  DO WHILE NOT cDpReady AND guard < 400
    SLEEP 50
    guard = guard + 1
  LOOP
  CdpCommand = cDpResult
END FUNCTION

FUNCTION BrowserSetFetchHeaders(headersList AS STRING, port AS INTEGER) AS STRING
  DIM ws AS WEBSOCKET
  DIM guard AS INTEGER
  cDpFetchHeaders = headersList
  cDpFetchEnabled = 1
  WEBSOCKET CONNECT "127.0.0.1", port, ws, CdpPath(port)
  IF ws IS NOTHING THEN
    BrowserSetFetchHeaders = "ERROR: CDP non raggiungibile"
    EXIT FUNCTION
  END IF
  ws.OnMessage = "CdpFetchHandler"
  cDpReady = 0
  ws.Send("{""id"":30,""method"":""Fetch.enable"",""params"":{""patterns"":[{""urlPattern"":""*""}]}}")
  guard = 0
  DO WHILE NOT cDpReady AND guard < 200
    SLEEP 50
    guard = guard + 1
  LOOP
  BrowserSetFetchHeaders = cDpResult
END FUNCTION

FUNCTION BrowserNavigate(url AS STRING, port AS INTEGER) AS STRING
  BrowserNavigate = CdpCommand("{""id"":1,""method"":""Page.navigate"",""params"":{""url"":" + CHR$(34) + url + CHR$(34) + "}}", port)
END FUNCTION

FUNCTION BrowserText(port AS INTEGER) AS STRING
  DIM raw AS STRING
  raw = CdpCommand("{""id"":3,""method"":""Runtime.evaluate"",""params"":{""expression"":" + CHR$(34) + "document.body.innerText" + CHR$(34) + ",""returnByValue"":true}}", port)
  BrowserText = CdpJsonStringValue(raw, "value")
END FUNCTION

FUNCTION BrowserSetCookie(name AS STRING, value AS STRING, url AS STRING, port AS INTEGER) AS STRING
  BrowserSetCookie = CdpCommand("{""id"":23,""method"":""Network.setCookie"",""params"":{""name"":" + CHR$(34) + name + CHR$(34) + ",""value"":" + CHR$(34) + value + CHR$(34) + ",""url"":" + CHR$(34) + url + CHR$(34) + "}}", port)
END FUNCTION

SUB Main()
  DIM body AS STRING
  BrowserSetCookie("session", "abc123", "http://127.0.0.1:8099", 9222)
  PRINT "FETCHHDR="; BrowserSetFetchHeaders("[{""name"":""X-Custom"",""value"":""fetch-d3""}]", 9222)
  BrowserNavigate("http://127.0.0.1:8099/", 9222)
  SLEEP 1500
  body = BrowserText(9222)
  PRINT "ECHO="; body
  PRINT "VERIFICA=COOKIE:"; INSTR(body, "abc123") > 0; " HDR:"; INSTR(body, "fetch-d3") > 0
  SLEEP 4000
END SUB
