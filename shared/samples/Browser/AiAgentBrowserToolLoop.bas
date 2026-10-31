' FILE: Browser/AiAgentBrowserToolLoop.bas
' B5 del piano automazione: ROUND-TRIP agente → tool del plugin browser (browser_navigate →
' browser_text) su una PAGINA REALE via CDP. L'agente (provider fixture) emette la tool-call,
' il loop la invoca (IlMcpToolDispatcher → InvokeBasicProcedure → la procedura del sample),
' e la risposta finale contiene il TESTO ESTRATTO dalla pagina reale.
'
' PREREQUISITI: Chromium headless avviato su :9222 (plugins/browser/launch_chromium.sh);
' la tool-call passa per il backend IL (su PCodeVM l'invoke-by-name non è implementato —
' errore esplicito).

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

FUNCTION CdpCommand(cmd AS STRING, port AS INTEGER) AS STRING
  DIM ws AS WEBSOCKET
  DIM guard AS INTEGER
  DIM cdpPath AS STRING
  cdpPath = CdpPath(port)
  WEBSOCKET CONNECT "127.0.0.1", port, ws, cdpPath
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

FUNCTION BrowserText(port AS INTEGER) AS STRING
  DIM raw AS STRING
  raw = CdpCommand("{""id"":3,""method"":""Runtime.evaluate"",""params"":{""expression"":" + CHR$(34) + "document.body.innerText" + CHR$(34) + ",""returnByValue"":true}}", port)
  BrowserText = JsonStringValue(raw, "value")
END FUNCTION

SUB Main()
  DIM answer$ AS STRING

  AI PLUGINS ENABLE "browser"
  PRINT "Plugin browser abilitato"

  AI AGENT CREATE "WebAgent"
  AI AGENT REGISTER TOOL "WebAgent", "browser_navigate"
  AI AGENT REGISTER TOOL "WebAgent", "browser_text"
  PRINT "Agent + tool registrati"

  answer$ = AI.AGENTASKSYNC("WebAgent", "tool: browser")
  PRINT "ANSWER="; answer$

  PRINT "VERIFICA="; INSTR(answer$, "Example") > 0

  AI AGENT DELETE "WebAgent"
END SUB
