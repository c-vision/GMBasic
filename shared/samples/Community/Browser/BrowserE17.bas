IMPORT AI WITH ALIAS
IMPORT NET
IMPORT HTTP
IMPORT STRINGS
IMPORT CRYPTOGRAPHY
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
FUNCTION BrowserClick(port AS INTEGER, selector AS STRING) AS STRING
  DIM js AS STRING
  js = "document.querySelector('" + selector + "').click(); 'clicked'"
  BrowserClick = CdpCommand("{""id"":60,""method"":""Runtime.evaluate"",""params"":{""expression"":" + CHR$(34) + js + CHR$(34) + ",""returnByValue"":true}}", port)
END FUNCTION
FUNCTION BrowserFill(port AS INTEGER, selector AS STRING, value AS STRING) AS STRING
  DIM js AS STRING
  js = "var e=document.querySelector('" + selector + "'); e.value='" + value + "'; e.dispatchEvent(new Event('input',{bubbles:true})); 'filled'"
  BrowserFill = CdpCommand("{""id"":61,""method"":""Runtime.evaluate"",""params"":{""expression"":" + CHR$(34) + js + CHR$(34) + ",""returnByValue"":true}}", port)
END FUNCTION
FUNCTION BrowserSessionRecord(port AS INTEGER, action AS STRING, detail AS STRING) AS STRING
  cDpSessionLog = cDpSessionLog + action + "|" + detail + CHR$(10)
  BrowserSessionRecord = "ok"
END FUNCTION
FUNCTION BrowserSessionLog(port AS INTEGER) AS STRING
  BrowserSessionLog = cDpSessionLog
END FUNCTION
FUNCTION BrowserReplay(port AS INTEGER, log AS STRING) AS STRING
  DIM pos AS INTEGER
  DIM eol AS INTEGER
  DIM line AS STRING
  DIM action AS STRING
  DIM detail AS STRING
  DIM done AS INTEGER
  DIM ok AS STRING
  pos = 1
  done = 0
  DO
    eol = INSTR(MID$(log, pos, LEN(log) - pos + 1), CHR$(10))
    IF eol = 0 THEN eol = LEN(log) - pos + 1
    line = MID$(log, pos, eol - 1)
    pos = pos + eol
    IF LEN(line) > 0 AND INSTR(line, "|") > 0 THEN
      action = MID$(line, 1, INSTR(line, "|") - 1)
      detail = MID$(line, INSTR(line, "|") + 1, LEN(line))
      IF action = "nav" THEN
        ok = BrowserNavigate(detail, port)
        SLEEP 400
      END IF
      IF action = "click" THEN
        ok = BrowserClick(port, detail)
        SLEEP 200
      END IF
      IF action = "type" THEN
        ok = BrowserFill(port, MID$(detail, 1, INSTR(detail, "|") - 1), MID$(detail, INSTR(detail, "|") + 1, LEN(detail)))
      END IF
      done = done + 1
    END IF
    IF pos >= LEN(log) THEN
      EXIT DO
    END IF
  LOOP
  BrowserReplay = "replayed " + LTRIM(STR$(done)) + " steps"
END FUNCTION

DIM port AS INTEGER = 9222
DIM rep AS STRING
DIM log AS STRING
DIM r AS STRING

PRINT "BrowserE17: avvio"

REM registra le interazioni (session log)
BrowserSessionRecord(port, "nav", "data:text/html,%3Cinput%20id%3D%27name%27%3E%3Cbutton%20id%3D%27go%27%20onclick%3D%22document.getElementById%28%27r%27%29.innerText%3D%27done%3A%27%2Bdocument.getElementById%28%27name%27%29.value%22%3EGo%3C%2Fbutton%3E%3Cdiv%20id%3D%27r%27%3E%3C%2Fdiv%3E")
BrowserSessionRecord(port, "type", "#name|gm")
BrowserSessionRecord(port, "click", "#go")
log = BrowserSessionLog(port)
PRINT "E17 recorded:"; LEN(log) > 0

REM riproduce deterministicamente le azioni registrate
rep = BrowserReplay(port, log)
PRINT "E17 replay:"; rep
IF INSTR(rep, "3 steps") > 0 THEN
  PRINT "E17 replay: ok (3 step riprodotti)"
ELSE
  PRINT "E17 replay: FALLITO"
END IF

REM verifica: dopo il replay il risultato deve essere 'done:gm'
SLEEP 400
r = BrowserText(port)
PRINT "E17 result:"; INSTR(r, "done:gm") > 0
IF INSTR(r, "done:gm") > 0 THEN
  PRINT "E17: ok (replay deterministico funziona)"
ELSE
  PRINT "E17: FALLITO"
END IF

PRINT "BrowserE17: fine"
