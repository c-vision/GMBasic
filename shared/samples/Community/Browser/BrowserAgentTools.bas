' FILE: Browser/BrowserAgentTools.bas
' D3 — l'AGENTE usa i tool del plugin browser: l'agente (fixture) emette la tool-call
' browser_set_cookie → il tool-loop la invoca (IlMcpToolDispatcher → InvokeBasicProcedureGpm →
' la procedura BrowserSetCookie del sample) → il cookie viene impostato sul Chromium CDP.
' Dimostra il percorso completo agente → tool browser (D3).
'
' PREREQUISITI: Chromium headless su :9222 (plugins/browser/launch_chromium.sh).

IMPORT AI WITH ALIAS
IMPORT NET
IMPORT HTTP
IMPORT STRINGS

DIM cDpResult AS STRING
DIM cDpReady AS BIT

SUB CdpHandler(ws AS WEBSOCKET, msg$)
  cDpResult = msg$
  cDpReady = 1
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

FUNCTION BrowserSetCookie(name AS STRING, value AS STRING, url AS STRING, port AS INTEGER) AS STRING
  BrowserSetCookie = CdpCommand("{""id"":23,""method"":""Network.setCookie"",""params"":{""name"":" + CHR$(34) + name + CHR$(34) + ",""value"":" + CHR$(34) + value + CHR$(34) + ",""url"":" + CHR$(34) + url + CHR$(34) + "}}", port)
END FUNCTION

FUNCTION BrowserGetCookies(port AS INTEGER) AS STRING
  BrowserGetCookies = CdpCommand("{""id"":24,""method"":""Network.getAllCookies""}", port)
END FUNCTION

SUB Main()
  DIM answer$ AS STRING
  DIM cookies AS STRING

  AI PLUGINS ENABLE "browser"
  PRINT "Plugin browser abilitato (tool D3 registrati)"

  AI AGENT CREATE "D3Agent"
  PRINT "Agent creato (i tool del plugin browser sono di DEFAULT — nessun REGISTER TOOL)"

  answer$ = AI.AGENTASKSYNC("D3Agent", "tool: cookie")
  PRINT "ANSWER="; answer$

  cookies = BrowserGetCookies(9222)
  PRINT "VERIFICA="; INSTR(cookies, "agent-cookie") > 0

  AI AGENT DELETE "D3Agent"
END SUB
