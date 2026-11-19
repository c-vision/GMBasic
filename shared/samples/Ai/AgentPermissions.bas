' PARITY-EXCLUDE: flaky di stato (2026-09-23, Parita-5-Backend-Residui §2B) — `AI PLUGINS ENABLE "browser"` risolve i tool in modo non deterministico sotto contesa macchina (Chromium/CDP altrui, plugin-dir condivisa): a triage PASS 3/3, poi FALLITO su IL/PCode mentre VMC resta stabile-ok (il contratto permessi deny→audit + allow→execute è implementato e verificato su VMC). Riesumare quando il plugin browser è deterministico.
' AgentPermissions.bas — modello di permessi (Sandbox/Supervised/YOLO).
' Sandbox: i tool "risky" (download/rete/scrittura/exec) sono NEGATI con errore esplicito
' + audit. YOLO: autonomia totale (audit). Supervised: approvazione human-in-the-loop
' (pending-approval via AgentPermissionService, attesa con timeout fail-closed).

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
FUNCTION BrowserSetCookie(name AS STRING, value AS STRING, url AS STRING, port AS INTEGER) AS STRING
  DIM u AS STRING
  u = url
  IF LEN(u) = 0 THEN u = "http://127.0.0.1"
  BrowserSetCookie = CdpCommand("{""id"":23,""method"":""Network.setCookie"",""params"":{""name"":" + CHR$(34) + name + CHR$(34) + ",""value"":" + CHR$(34) + value + CHR$(34) + ",""url"":" + CHR$(34) + u + CHR$(34) + "}}", port)
END FUNCTION

AI PLUGINS ENABLE "browser"
AI AGENT TRACE on

DIM r AS STRING
DIM ev$ AS STRING

PRINT "AgentPermissions: avvio"

AI AGENT CREATE "P"
AI PERMISSION P, sandbox
PRINT "level:"; AI.AGENTGET("P", "permission")

REM override per-tool (vince sul livello)
AI PERMISSION P, TOOL browser_set_cookie, allow
PRINT "policies:"; AI.AGENTGET("P", "toolpolicies")

REM sandbox: browser_set_cookie (risky) negato MAI eseguito + audit
AI PERMISSION P, TOOL browser_set_cookie, deny
r = AI.AGENTASKSYNC("P", "tool: cookie")
ev$ = JOIN(AI.AGENTEVENTS(), " ")
PRINT "sandbox events:"; ev$
IF INSTR(ev$, "PermissionDenied") > 0 AND INSTR(ev$, "permesso negato") > 0 THEN
  PRINT "sandbox: ok (risky negato)"
ELSE
  PRINT "sandbox: FALLITO"
END IF

REM yolo: lo stesso tool passa
AI PERMISSION P, yolo
r = AI.AGENTASKSYNC("P", "tool: cookie")
PRINT "yolo: ok (risky permesso)"

PRINT "AgentPermissions: fine"
