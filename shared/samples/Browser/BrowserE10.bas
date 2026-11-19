IMPORT AI WITH ALIAS
IMPORT NET
IMPORT HTTP
IMPORT STRINGS
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY
DIM cDpResult AS STRING
DIM cDpReady AS BIT
DIM cDpCmdTimeout AS INTEGER
cDpCmdTimeout = 200
SUB CdpHandler(ws AS WEBSOCKET, msg$)
  cDpResult = msg$
  cDpReady = 1
END SUB
FUNCTION CdpPath(port AS INTEGER) AS STRING
  ' Scopre il path del WebSocket CDP dal /json (webSocketDebuggerUrl).
  DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:" + LTRIM(STR$(port)) + "/json")
  DIM j$ AS STRING
  DIM prefix$ AS STRING
  DIM p1 AS INTEGER
  DIM p2 AS INTEGER
  j$ = r.Body
  prefix$ = "ws://127.0.0.1:" + LTRIM(STR$(port))
  p1 = INSTR(j$, prefix$)
  IF p1 = 0 THEN
    CdpPath = "/devtools/page/1"   ' fallback (raro)
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
  needle = CHR$(34) + key + CHR$(34) + ":"
  p1 = INSTR(raw, needle)
  IF p1 = 0 THEN
    CdpJsonStringValue = ""
    EXIT FUNCTION
  END IF
  p1 = p1 + LEN(needle)
  DO WHILE MID$(raw, p1, 1) = " " AND p1 < LEN(raw)
    p1 = p1 + 1
  LOOP
  IF MID$(raw, p1, 1) <> CHR$(34) THEN
    CdpJsonStringValue = ""
    EXIT FUNCTION
  END IF
  p1 = p1 + 1
  p2 = INSTR(MID$(raw, p1, LEN(raw) - p1 + 1), CHR$(34))
  IF p2 = 0 THEN
    CdpJsonStringValue = MID$(raw, p1, LEN(raw) - p1 + 1)
  ELSE
    CdpJsonStringValue = MID$(raw, p1, p2 - 1)
  END IF
END FUNCTION
FUNCTION CdpCommand(cmd AS STRING, port AS INTEGER) AS STRING
  DIM ws AS WEBSOCKET
  DIM guard AS INTEGER
  WEBSOCKET CONNECT "127.0.0.1", port, ws, CdpPath(port), 8000
  IF ws IS NOTHING THEN
    CdpCommand = "ERROR: CDP non raggiungibile"
    EXIT FUNCTION
  END IF
  ws.OnMessage = "CdpHandler"
  cDpReady = 0
  ws.Send(cmd)
  guard = 0
  DO WHILE NOT cDpReady AND guard < cDpCmdTimeout
    SLEEP 50
    guard = guard + 1
  LOOP
  IF NOT cDpReady THEN
    CdpCommand = "ERROR: CDP timeout sul comando"
    EXIT FUNCTION
  END IF
  CdpCommand = cDpResult
END FUNCTION
FUNCTION BrowserNavigate(url AS STRING, port AS INTEGER) AS STRING
  DIM ws AS WEBSOCKET
  WEBSOCKET CONNECT "127.0.0.1", port, ws, CdpPath(port), 8000
  ws.OnMessage = "CdpHandler"
  cDpReady = 0
  ws.Send("{""id"":1,""method"":""Page.navigate"",""params"":{""url"":" + CHR$(34) + url + CHR$(34) + "}}")
  DO WHILE NOT cDpReady
    SLEEP 50
  LOOP
  BrowserNavigate = cDpResult
END FUNCTION
FUNCTION BrowserEvaluate(js AS STRING, port AS INTEGER) AS STRING
  DIM ws AS WEBSOCKET
  WEBSOCKET CONNECT "127.0.0.1", port, ws, CdpPath(port), 8000
  ws.OnMessage = "CdpHandler"
  cDpReady = 0
  ws.Send("{""id"":4,""method"":""Runtime.evaluate"",""params"":{""expression"":" + CHR$(34) + js + CHR$(34) + ",""returnByValue"":true}}")
  DO WHILE NOT cDpReady
    SLEEP 50
  LOOP
  BrowserEvaluate = cDpResult
END FUNCTION
FUNCTION BrowserProfile(port AS INTEGER, ua AS STRING, width AS INTEGER, height AS INTEGER) AS STRING
  DIM raw AS STRING
  raw = CdpCommand("{""id"":95,""method"":""Network.setUserAgentOverride"",""params"":{""userAgent"":" + CHR$(34) + ua + CHR$(34) + ",""acceptLanguage"":""it-IT,it;q=0.9"",""userAgentMetadata"":{""brands"":[{""brand"":""Chromium"",""version"":""149""},{""brand"":""Not(A:Brand"",""version"":""8""}],""fullVersion"":""149.0.7827.55"",""platform"":""macOS"",""platformVersion"":""15.0"",""architecture"":""arm64"",""model"":"""",""mobile"":false,""bitness"":""64""}}}", port)
  raw = CdpCommand("{""id"":96,""method"":""Emulation.setDeviceMetricsOverride"",""params"":{""width"":" + LTRIM(STR$(width)) + ",""height"":" + LTRIM(STR$(height)) + ",""deviceScaleFactor"":1,""mobile"":false}}", port)
  raw = CdpCommand("{""id"":97,""method"":""Page.addScriptToEvaluateOnNewDocument"",""params"":{""source"":""Object.defineProperty(navigator,'webdriver',{get:()=>undefined})""}}", port)
  BrowserProfile = "ok"
END FUNCTION

DIM port AS INTEGER = 9222
DIM nav AS STRING
DIM p AS STRING
DIM r AS STRING

PRINT "BrowserE10: avvio"

REM profilo PRIMA della navigazione: lo script anti-webdriver vale per i nuovi documenti
p = BrowserProfile(port, "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36", 1280, 720)
PRINT "E10 profile:"; p

nav = BrowserNavigate("data:text/html,<body>E10</body>", port)
SLEEP 600

REM verifica UA
r = BrowserEvaluate("navigator.userAgent", port)
PRINT "E10 ua:"; INSTR(r, "Chrome/149") > 0
IF INSTR(r, "Chrome/149") > 0 THEN
  PRINT "E10 UA: ok"
ELSE
  PRINT "E10 UA: FALLITO"
END IF

REM verifica viewport
r = BrowserEvaluate("window.innerWidth + 'x' + window.innerHeight", port)
PRINT "E10 viewport:"; INSTR(r, "1280x720") > 0

REM verifica anti-fingerprint (navigator.webdriver rimosso)
r = BrowserEvaluate("navigator.webdriver", port)
PRINT "E10 webdriver value:"; MID$(r, 1, 60)
IF INSTR(r, "true") = 0 THEN
  PRINT "E10 anti-webdriver: ok (webdriver non e' un marker)"
ELSE
  PRINT "E10 anti-webdriver: FALLITO (navigator.webdriver=true)"
END IF

PRINT "BrowserE10: fine"
