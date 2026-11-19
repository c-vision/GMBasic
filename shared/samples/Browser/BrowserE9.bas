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
FUNCTION HumanDelay(minMs AS INTEGER, maxMs AS INTEGER) AS INTEGER
  DIM span AS INTEGER
  DIM d AS INTEGER
  span = maxMs - minMs
  IF span < 1 THEN span = 1
  d = minMs + RANDOMINT(0, span)
  SLEEP d
  HumanDelay = d
END FUNCTION
FUNCTION HumanClick(port AS INTEGER, selector AS STRING) AS STRING
  DIM pos AS STRING
  DIM x AS INTEGER
  DIM y AS STRING
  DIM steps AS INTEGER
  DIM i AS INTEGER
  DIM t AS INTEGER
  DIM w AS INTEGER
  pos = CdpJsonStringValue(BrowserEvaluate("var r=document.querySelector('" + selector + "').getBoundingClientRect(); Math.round(r.x+r.width/2)+'|'+Math.round(r.y+r.height/2)", port), "value")
  IF INSTR(pos, "|") = 0 THEN
    HumanClick = "ERROR: selettore non trovato"
    EXIT FUNCTION
  END IF
  x = VAL(MID$(pos, 1, INSTR(pos, "|") - 1))
  y = MID$(pos, INSTR(pos, "|") + 1, LEN(pos))
  w = HumanDelay(200, 800)
  steps = 5
  FOR i = 1 TO steps
    CdpCommand("{""id"":70,""method"":""Input.dispatchMouseEvent"",""params"":{""type"":""mouseMoved"",""x"":" + LTRIM(STR$(x * i / steps)) + ",""y"":" + LTRIM(STR$(VAL(y) * i / steps)) + "}}", port)
    SLEEP 30
  NEXT i
  CdpCommand("{""id"":71,""method"":""Input.dispatchMouseEvent"",""params"":{""type"":""mousePressed"",""x"":" + LTRIM(STR$(x)) + ",""y"":" + LTRIM(STR$(VAL(y))) + ",""button"":""left"",""clickCount"":1}}", port)
  SLEEP 50
  CdpCommand("{""id"":72,""method"":""Input.dispatchMouseEvent"",""params"":{""type"":""mouseReleased"",""x"":" + LTRIM(STR$(x)) + ",""y"":" + LTRIM(STR$(VAL(y))) + ",""button"":""left"",""clickCount"":1}}", port)
  HumanClick = "ok"
END FUNCTION
FUNCTION HumanType(port AS INTEGER, selector AS STRING, text AS STRING) AS STRING
  DIM i AS INTEGER
  DIM ch AS STRING
  BrowserEvaluate("document.querySelector('" + selector + "').focus()", port)
  FOR i = 1 TO LEN(text)
    ch = MID$(text, i, 1)
    CdpCommand("{""id"":73,""method"":""Input.insertText"",""params"":{""text"":" + CHR$(34) + ch + CHR$(34) + "}}", port)
    w = HumanDelay(30, 120)
  NEXT i
  HumanType = "ok"
END FUNCTION
FUNCTION HumanScroll(port AS INTEGER, dy AS INTEGER, steps AS INTEGER) AS STRING
  DIM per AS INTEGER
  DIM i AS INTEGER
  DIM w AS INTEGER
  IF steps < 1 THEN steps = 1
  per = dy / steps
  FOR i = 1 TO steps
    BrowserEvaluate("window.scrollBy(0, " + LTRIM(STR$(per)) + ")", port)
    w = HumanDelay(40, 120)
  NEXT i
  HumanScroll = "ok"
END FUNCTION

DIM port AS INTEGER = 9222
DIM nav AS STRING
DIM ok AS STRING
DIM r AS STRING

PRINT "BrowserE9: avvio"

nav = BrowserNavigate("data:text/html,%3Ch1%20id%3D%22t%22%3EE9%3C%2Fh1%3E%3Cinput%20id%3D%22name%22%3E%3Cbutton%20id%3D%22go%22%20onclick%3D%22document.getElementById%28%27r%27%29.innerText%3D%27done%3A%27%2Bdocument.getElementById%28%27name%27%29.value%22%3EGo%3C%2Fbutton%3E%3Cdiv%20id%3D%22r%22%3E%3C%2Fdiv%3E%3Cdiv%20style%3D%22height%3A1500px%22%3E%3C%2Fdiv%3E", port)
SLEEP 700
PRINT "E9 nav:"; LEN(nav) > 0

ok = HumanType(port, "#name", "gm")
PRINT "E9 type:"; ok

ok = HumanClick(port, "#go")
PRINT "E9 click:"; ok

SLEEP 400
r = BrowserText(port)
PRINT "E9 result:"; INSTR(r, "done:gm") > 0
IF INSTR(r, "done:gm") > 0 THEN
  PRINT "E9: ok (click umano + typing funzionano)"
ELSE
  PRINT "E9: FALLITO"
END IF

ok = HumanScroll(port, 500, 5)
PRINT "E9 scroll:"; ok
r = BrowserEvaluate("window.scrollY", port)
PRINT "E9 scrollY:"; INSTR(r, "500") > 0

PRINT "BrowserE9: fine"
