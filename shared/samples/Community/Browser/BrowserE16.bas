IMPORT AI WITH ALIAS
IMPORT NET
IMPORT HTTP
IMPORT STRINGS
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
FUNCTION BrowserRobotsAllowed(url AS STRING) AS STRING
  DIM host AS STRING
  DIM path AS STRING
  DIM r AS FETCHRESULT
  DIM body AS STRING
  DIM pos AS INTEGER
  DIM eol AS INTEGER
  DIM line AS STRING
  DIM dis AS STRING
  DIM p AS STRING
  host = "http://" + MID$(url, 8, INSTR(MID$(url, 8, LEN(url) - 7), "/") - 1)
  r = FETCH(host + "/robots.txt")
  body = r.Body
  IF LEN(body) = 0 THEN
    BrowserRobotsAllowed = "allowed"
    EXIT FUNCTION
  END IF
  p = INSTR(MID$(url, 8, LEN(url) - 7), "/")
  IF p > 0 THEN path = MID$(url, 7 + p, LEN(url))
  pos = 1
  DO
    eol = INSTR(MID$(body, pos, LEN(body) - pos + 1), CHR$(10))
    IF eol = 0 THEN eol = LEN(body) - pos + 2
    line = MID$(body, pos, eol - 1)
    pos = pos + eol
    IF INSTR(line, "Disallow:") > 0 THEN
      dis = LTRIM(MID$(line, INSTR(line, ":") + 1, LEN(line)))
      IF LEN(dis) > 0 AND LEN(path) > 0 AND LEN(dis) <= LEN(path) THEN
        IF MID$(path, 1, LEN(dis)) = dis THEN
          BrowserRobotsAllowed = "blocked: " + dis
          EXIT FUNCTION
        END IF
      END IF
    END IF
    IF pos >= LEN(body) THEN
      EXIT DO
    END IF
  LOOP
  BrowserRobotsAllowed = "allowed"
END FUNCTION
FUNCTION BrowserRateLimit(minMs AS INTEGER) AS STRING
  SLEEP minMs
  BrowserRateLimit = "ok"
END FUNCTION
FUNCTION BrowserCheckCaptcha(port AS INTEGER) AS STRING
  DIM raw AS STRING
  DIM t AS STRING
  raw = BrowserText(port)
  t = CdpJsonStringValue(raw, "value")
  IF INSTR(t, "captcha") > 0 OR INSTR(t, "recaptcha") > 0 OR INSTR(t, "verify you are human") > 0 THEN
    BrowserCheckCaptcha = "captcha"
  ELSE
    BrowserCheckCaptcha = ""
  END IF
END FUNCTION

DIM port AS INTEGER = 9222
DIM r AS STRING
DIM nav AS STRING

PRINT "BrowserE16: avvio"

REM robots.txt: /private/ disallowato -> blocked; /page16.html -> allowed
r = BrowserRobotsAllowed("http://127.0.0.1:8107/private/data.x")
PRINT "E16 robots private:"; r
IF INSTR(r, "blocked") > 0 THEN
  PRINT "E16 robots: ok (rispetta Disallow)"
ELSE
  PRINT "E16 robots: FALLITO (atteso blocked)"
END IF

r = BrowserRobotsAllowed("http://127.0.0.1:8107/page16.html")
PRINT "E16 robots page:"; r
IF INSTR(r, "allowed") > 0 THEN
  PRINT "E16 robots page: ok"
ELSE
  PRINT "E16 robots page: FALLITO"
END IF

REM rate-limit (cortesia)
r = BrowserRateLimit(300)
PRINT "E16 ratelimit:"; r

REM CAPTCHA detection: pagina con "verify you are human"
nav = BrowserNavigate("http://127.0.0.1:8107/captcha.html", port)
SLEEP 700
r = BrowserCheckCaptcha(port)
PRINT "E16 captcha:"; r
IF INSTR(r, "captcha") > 0 THEN
  PRINT "E16 captcha: ok (rilevato, l'agente si ferma)"
ELSE
  PRINT "E16 captcha: FALLITO"
END IF

PRINT "BrowserE16: fine"
