IMPORT AI WITH ALIAS
IMPORT NET
IMPORT HTTP
IMPORT STRINGS
IMPORT CRYPTOGRAPHY
DIM cDpResult AS STRING
DIM cDpReady AS BIT
DIM cDpCmdTimeout AS INTEGER
DIM cDpSessionLog AS STRING
DIM cDpStepCount AS INTEGER
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
FUNCTION BrowserAuditStep(port AS INTEGER, action AS STRING, detail AS STRING, shotDir AS STRING) AS STRING
  DIM shot AS STRING
  DIM raw AS STRING
  DIM b64 AS STRING
  DIM n AS INTEGER
  cDpStepCount = cDpStepCount + 1
  n = cDpStepCount
  shot = shotDir + "/step-" + LTRIM(STR$(n)) + ".png"
  raw = BrowserScreenshot(shot, port)
  b64 = CdpJsonStringValue(raw, "data")
  IF LEN(b64) > 0 THEN
    OPEN shot FOR BINARY AS #1
    PUT #1, , FROMBASE64(b64)
    CLOSE #1
  END IF
  cDpSessionLog = cDpSessionLog + action + "|" + detail + "|" + shot + CHR$(10)
  BrowserAuditStep = shot
END FUNCTION
FUNCTION BrowserAuditExport(port AS INTEGER, outDir AS STRING) AS STRING
  DIM log AS STRING
  log = cDpSessionLog
  OPEN outDir + "/audit.log" FOR OUTPUT AS #1
  PRINT #1, log
  CLOSE #1
  BrowserAuditExport = log
END FUNCTION

DIM port AS INTEGER = 9222
DIM nav AS STRING
DIM shot AS STRING
DIM log AS STRING
DIM r AS STRING

PRINT "BrowserE12: avvio"

nav = BrowserNavigate("data:text/html,%3Ch1%20id%3D%27t%27%3EE12%3C%2Fh1%3E%3Cbutton%20id%3D%27b%27%3EGo%3C%2Fbutton%3E%3Cdiv%20id%3D%27r%27%3E%3C%2Fdiv%3E", port)
SLEEP 600

shot = BrowserAuditStep(port, "nav", "pagina-E12", "work/audit")
PRINT "E12 shot1:"; LEN(shot) > 0
r = BrowserEvaluate("document.getElementById('r').innerText='clicked'", port)
shot = BrowserAuditStep(port, "click", "#b", "work/audit")
PRINT "E12 shot2:"; LEN(shot) > 0

log = BrowserAuditExport(port, "work/audit")
PRINT "E12 log:"; CHR$(10); log
IF INSTR(log, "step-1.png") > 0 AND INSTR(log, "step-2.png") > 0 THEN
  PRINT "E12: ok (log con prove visive a ogni step)"
ELSE
  PRINT "E12: FALLITO"
END IF

PRINT "BrowserE12: fine"
