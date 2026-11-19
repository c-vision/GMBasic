' FILE: Browser/BrowserPluginTest.bas
' BrowserPluginTest — B3 del piano automazione: il plugin browser (plugins/browser/) su una
' PAGINA REALE via Chrome DevTools Protocol (CDP). Chromium headless avviato con
' --remote-debugging-port (helper plugins/browser/launch_chromium.sh, porta 9222 default).
'
' FLUSSO: il plugin espone le procedure BrowserNavigate/BrowserTitle/BrowserText/
' BrowserEvaluate/BrowserScreenshot; questo sample le IMPLEMENTA (come i sample AiPlugins/
' AiAgentToolLoop implementano le procedure del plugin web-fetch) e le usa per navigare
' https://example.com, leggere titolo/testo, eseguire JS e salvare uno screenshot PNG in
' work/shot.png (FROMBASE64 + PUT #1 FOR BINARY). B4: AI PLUGINS ENABLE "browser" registra
' i tool (browser_navigate/...) nel McpToolRegistry.
'
' NOTA IL-backend: la connessione WS client verso un host esterno (CDP) ha una race nota
' (vedi AI-Agent-Automation-TODO.md B3) — per la verifica usa il backend PCodeVM
' (--engine=pcode), dove il flusso e' affidabile end-to-end.

IMPORT AI WITH ALIAS
IMPORT NET
IMPORT HTTP
IMPORT STRINGS
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY

DIM G_PORT AS INTEGER = 9222
DIM cDpResult AS STRING
DIM cDpReady AS BIT

SUB CdpHandler(ws AS WEBSOCKET, msg$)
  cDpResult = msg$
  cDpReady = 1
END SUB

' Estrae il valore STRINGA del campo key$ da un JSON CDP ("key":"valore").
FUNCTION JsonStringValue(raw AS STRING, key AS STRING) AS STRING
  DIM needle AS STRING
  DIM p1 AS INTEGER
  DIM p2 AS INTEGER
  needle = CHR$(34) + key + CHR$(34) + ":" + CHR$(34)
  p1 = INSTR(raw, needle)
  IF p1 = 0 THEN
    JsonStringValue = ""
  ELSE
    p1 = p1 + LEN(needle)
    p2 = INSTR(MID$(raw, p1, LEN(raw) - p1 + 1), CHR$(34))
    IF p2 = 0 THEN
      JsonStringValue = MID$(raw, p1, LEN(raw) - p1 + 1)
    ELSE
      JsonStringValue = MID$(raw, p1, p2 - 1)
    END IF
  END IF
END FUNCTION

' Scopre il path WebSocket CDP della prima pagina dal /json (webSocketDebuggerUrl).
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

' Invia un comando CDP e attende la risposta su ws.OnMessage (bounded guard).
FUNCTION CdpCommand(cmd AS STRING, port AS INTEGER) AS STRING
  DIM ws AS WEBSOCKET
  DIM guard AS INTEGER
  WEBSOCKET CONNECT "127.0.0.1", port, ws, CdpPath(port)
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

FUNCTION BrowserNavigate(url AS STRING, port AS INTEGER) AS STRING
  BrowserNavigate = CdpCommand("{""id"":1,""method"":""Page.navigate"",""params"":{""url"":" + CHR$(34) + url + CHR$(34) + "}}", port)
END FUNCTION

FUNCTION BrowserTitle(port AS INTEGER) AS STRING
  DIM raw AS STRING
  raw = CdpCommand("{""id"":2,""method"":""Runtime.evaluate"",""params"":{""expression"":" + CHR$(34) + "document.title" + CHR$(34) + ",""returnByValue"":true}}", port)
  BrowserTitle = JsonStringValue(raw, "value")
END FUNCTION

FUNCTION BrowserText(port AS INTEGER) AS STRING
  DIM raw AS STRING
  raw = CdpCommand("{""id"":3,""method"":""Runtime.evaluate"",""params"":{""expression"":" + CHR$(34) + "document.body.innerText" + CHR$(34) + ",""returnByValue"":true}}", port)
  BrowserText = JsonStringValue(raw, "value")
END FUNCTION

FUNCTION BrowserEvaluate(js AS STRING, port AS INTEGER) AS STRING
  DIM raw AS STRING
  raw = CdpCommand("{""id"":4,""method"":""Runtime.evaluate"",""params"":{""expression"":" + CHR$(34) + js + CHR$(34) + ",""returnByValue"":true}}", port)
  BrowserEvaluate = JsonStringValue(raw, "value")
END FUNCTION

FUNCTION BrowserScreenshot(port AS INTEGER) AS STRING
  DIM raw AS STRING
  raw = CdpCommand("{""id"":5,""method"":""Page.captureScreenshot"",""params"":{""format"":""png""}}", port)
  BrowserScreenshot = JsonStringValue(raw, "data")
END FUNCTION

SUB Main()
  DIM b64 AS STRING
  DIM title$ AS STRING
  DIM body$ AS STRING
  DIM h1$ AS STRING
  DIM shot$ AS STRING
  DIM shotLen AS INTEGER

  AI PLUGINS ENABLE "browser"
  PRINT "Plugin browser abilitato (tool registrati nel McpToolRegistry)"

  MKDIR "work"

  BrowserNavigate("https://example.com", G_PORT)
  PRINT "Navigato a https://example.com"

  SLEEP 500

  title$ = BrowserTitle(G_PORT)
  PRINT "TITOLO="; title$

  body$ = BrowserText(G_PORT)
  PRINT "TESTOLEN="; LEN(body$)

  h1$ = BrowserEvaluate("document.querySelector('h1').innerText", G_PORT)
  PRINT "H1="; h1$

  b64 = BrowserScreenshot(G_PORT)
  OPEN "work/shot.png" FOR BINARY AS #1
  DIM b64tmp(LEN(b64)) AS BYTE
  DIM nB64dec AS LONG = FROMBASE64(b64, b64tmp)
  DIM b64buf(nB64dec - 1) AS BYTE
  DIM b64i AS LONG
  FOR b64i = 0 TO nB64dec - 1 : b64buf(b64i) = b64tmp(b64i) : NEXT b64i
  PUT #1, , b64buf
  CLOSE #1
  PRINT "SHOTLEN="; LEN(b64)

  PRINT "VERIFICA:"
  PRINT "  titolo contiene Example ="; INSTR(title$, "Example") > 0
  PRINT "  testo non vuoto ="; LEN(body$) > 10
  PRINT "  h1 estratto ="; h1$ = "Example Domain"
END SUB
