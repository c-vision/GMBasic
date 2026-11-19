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
FUNCTION BrowserCdpError(raw AS STRING) AS STRING
  DIM d AS STRING
  d = ""
  IF INSTR(raw, CHR$(34) + "error" + CHR$(34)) > 0 THEN
    IF INSTR(raw, CHR$(34) + "message" + CHR$(34)) > 0 THEN
      d = CdpJsonStringValue(raw, "message")
    END IF
    IF LEN(d) = 0 THEN
      d = "CDP protocol error"
    END IF
  ELSE
    IF INSTR(raw, "exceptionDetails") > 0 THEN
      IF INSTR(raw, CHR$(34) + "description" + CHR$(34)) > 0 THEN
        d = CdpJsonStringValue(raw, "description")
      END IF
      IF LEN(d) = 0 THEN
        d = "JS exception nella pagina"
      END IF
    END IF
  END IF
  BrowserCdpError = d
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
FUNCTION BrowserScreenshot(out AS STRING, port AS INTEGER) AS STRING
  DIM ws AS WEBSOCKET
  WEBSOCKET CONNECT "127.0.0.1", port, ws, CdpPath(port), 8000
  ws.OnMessage = "CdpHandler"
  cDpReady = 0
  ws.Send("{""id"":5,""method"":""Page.captureScreenshot"",""params"":{""format"":""png""}}")
  DO WHILE NOT cDpReady
    SLEEP 50
  LOOP
  BrowserScreenshot = cDpResult
END FUNCTION
FUNCTION BrowserAnalyze(port AS INTEGER, prompt AS STRING, outPath AS STRING) AS STRING
  DIM raw AS STRING
  DIM b64 AS STRING
  DIM err AS STRING
  raw = BrowserScreenshot(outPath, port)
  b64 = CdpJsonStringValue(raw, "data")
  IF LEN(b64) = 0 THEN
    err = BrowserCdpError(raw)
    IF LEN(err) > 0 THEN
      BrowserAnalyze = "ERROR: screenshot non riuscito (" + err + ")"
    ELSE
      BrowserAnalyze = "ERROR: screenshot non riuscito (nessun dato)"
    END IF
    EXIT FUNCTION
  END IF
  OPEN outPath FOR BINARY AS #1
  DIM b64tmp(LEN(b64)) AS BYTE
  DIM nB64dec AS LONG = FROMBASE64(b64, b64tmp)
  DIM b64buf(nB64dec - 1) AS BYTE
  DIM b64i AS LONG
  FOR b64i = 0 TO nB64dec - 1 : b64buf(b64i) = b64tmp(b64i) : NEXT b64i
  PUT #1, , b64buf
  CLOSE #1
  BrowserAnalyze = AI.ANALYZEIMAGE(outPath, prompt)
END FUNCTION
FUNCTION BrowserAssert(port AS INTEGER, assertion AS STRING, outPath AS STRING) AS STRING
  DIM shot AS STRING
  DIM verdict AS STRING
  shot = BrowserAnalyze(port, "ASSERT: " + assertion, outPath)
  verdict = LTRIM(shot)
  BrowserAssert = verdict
END FUNCTION

DIM port AS INTEGER = 9222
DIM v AS STRING
DIM nav AS STRING

PRINT "BrowserE8: avvio"

nav = BrowserNavigate("data:text/html,<button id=" + CHR$(34) + "btn" + CHR$(34) + ">Go</button>", port)
SLEEP 600
PRINT "E8 nav:"; LEN(nav) > 0

v = BrowserAssert(port, "il pulsante Go e' visibile e il layout e' corretto", "work/shot_e8.png")
PRINT "E8 assert true:"; v
IF INSTR(v, "true") > 0 THEN
  PRINT "E8: ok (asserzione vera)"
ELSE
  PRINT "E8: FALLITO (atteso true)"
END IF

v = BrowserAssert(port, "ASSERT:FALSE il layout e' rotto", "work/shot_e8b.png")
PRINT "E8 assert false:"; v
IF INSTR(v, "false") > 0 THEN
  PRINT "E8 false: ok"
ELSE
  PRINT "E8 false: FALLITO (atteso false)"
END IF

PRINT "BrowserE8: fine"
