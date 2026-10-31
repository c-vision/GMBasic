IMPORT AI WITH ALIAS
IMPORT NET
IMPORT HTTP
IMPORT STRINGS
IMPORT CRYPTOGRAPHY
IMPORT IO WITH ALIAS
DIM cDpResult AS STRING
DIM cDpReady AS BIT
DIM cDpCmdTimeout AS INTEGER
DIM cDpSessionLog AS STRING
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
FUNCTION BrowserText(port AS INTEGER) AS STRING
  DIM ws AS WEBSOCKET
  WEBSOCKET CONNECT "127.0.0.1", port, ws, CdpPath(port), 8000
  ws.OnMessage = "CdpHandler"
  cDpReady = 0
  ws.Send("{""id"":3,""method"":""Runtime.evaluate"",""params"":{""expression"":""document.body.innerText"",""returnByValue"":true}}")
  DO WHILE NOT cDpReady
    SLEEP 50
  LOOP
  BrowserText = cDpResult
END FUNCTION
FUNCTION BrowserSessionRecord(port AS INTEGER, action AS STRING, detail AS STRING) AS STRING
  cDpSessionLog = cDpSessionLog + action + "|" + detail + CHR$(10)
  BrowserSessionRecord = "ok"
END FUNCTION
FUNCTION BrowserSessionLog(port AS INTEGER) AS STRING
  BrowserSessionLog = cDpSessionLog
END FUNCTION
FUNCTION BrowserTaskSave(port AS INTEGER, filePath AS STRING) AS STRING
  DIM log AS STRING
  DIM url AS STRING
  DIM st AS STRING
  st = BrowserEvaluate("location.href", port)
  url = CdpJsonStringValue(st, "value")
  log = BrowserSessionLog(port)
  OPEN filePath FOR OUTPUT AS #1
  WRITE #1, url
  WRITE #1, log
  CLOSE #1
  BrowserTaskSave = "ok"
END FUNCTION
FUNCTION BrowserTaskResume(port AS INTEGER, filePath AS STRING) AS STRING
  DIM url AS STRING
  DIM log AS STRING
  OPEN filePath FOR INPUT AS #1
  INPUT #1, url
  INPUT #1, log
  CLOSE #1
  BrowserNavigate(url, port)
  BrowserTaskResume = log
END FUNCTION

DIM port AS INTEGER = 9222
DIM nav AS STRING
DIM log AS STRING
DIM r AS STRING

PRINT "BrowserE14: avvio"

nav = BrowserNavigate("data:text/html,%3Ch1%20id%3D%27t%27%3EE14%3C%2Fh1%3E%3Cbutton%20id%3D%27b%27%3EGo%3C%2Fbutton%3E", port)
SLEEP 600
BrowserSessionRecord(port, "nav", "pagina-E14")
BrowserSessionRecord(port, "click", "#b")
PRINT "E14 steps recorded:"; LEN(BrowserSessionLog(port)) > 0

r = BrowserTaskSave(port, "work/taskstate.txt")
PRINT "E14 save:"; r

nav = BrowserNavigate("about:blank", port)
SLEEP 300
log = BrowserTaskResume(port, "work/taskstate.txt")
PRINT "E14 resumed log:"; CHR$(10); log
IF INSTR(log, "pagina-E14") > 0 THEN
  PRINT "E14: ok (riprende da dove si era fermato)"
ELSE
  PRINT "E14: FALLITO"
END IF

r = BrowserEvaluate("location.href", port)
PRINT "E14 url:"; INSTR(r, "data:text/html") > 0

PRINT "BrowserE14: fine"
