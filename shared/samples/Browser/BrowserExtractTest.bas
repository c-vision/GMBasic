' FILE: Browser/BrowserExtractTest.bas
' C7 del piano automazione — estrazione STRUTTURATA: BrowserExtract(url, istruzioni, schema)
' naviga la pagina, ne legge il TESTO e chiede all'agente di estrarre i dati come da SCHEMA
' (JSON {"campo":"tipo"}), con validazione contro lo schema. La risposta è il JSON strutturato.
'
' PREREQUISITI: Chromium headless avviato su :9222 (plugins/browser/launch_chromium.sh);
' completer fixture (provider di default) per la verifica deterministica.

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

FUNCTION CdpJsonStringValue(raw AS STRING, key AS STRING) AS STRING
  DIM needle AS STRING
  DIM p1 AS INTEGER
  DIM p2 AS INTEGER
  needle = CHR$(34) + key + CHR$(34) + ":" + CHR$(34)
  p1 = INSTR(raw, needle)
  IF p1 = 0 THEN
    CdpJsonStringValue = ""
  ELSE
    p1 = p1 + LEN(needle)
    p2 = INSTR(MID$(raw, p1, LEN(raw) - p1 + 1), CHR$(34))
    IF p2 = 0 THEN
      CdpJsonStringValue = MID$(raw, p1, LEN(raw) - p1 + 1)
    ELSE
      CdpJsonStringValue = MID$(raw, p1, p2 - 1)
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

FUNCTION BrowserNavigate(url AS STRING, port AS INTEGER) AS STRING
  BrowserNavigate = CdpCommand("{""id"":1,""method"":""Page.navigate"",""params"":{""url"":" + CHR$(34) + url + CHR$(34) + "}}", port)
END FUNCTION

FUNCTION BrowserText(port AS INTEGER) AS STRING
  DIM raw AS STRING
  raw = CdpCommand("{""id"":3,""method"":""Runtime.evaluate"",""params"":{""expression"":" + CHR$(34) + "document.body.innerText" + CHR$(34) + ",""returnByValue"":true}}", port)
  BrowserText = CdpJsonStringValue(raw, "value")
END FUNCTION

FUNCTION BrowserExtract(url AS STRING, instruction AS STRING, schema AS STRING, port AS INTEGER) AS STRING
  DIM text AS STRING
  BrowserNavigate(url, port)
  SLEEP 600
  text = BrowserText(port)
  BrowserExtract = AI.AGENTASKSYNC("extract", "EXTRACT STRUCTURED SCHEMA: " + schema + " INSTRUCTION: " + instruction + " PAGE: " + text)
END FUNCTION

SUB Main()
  DIM res AS STRING
  AI AGENT CREATE "extract"
  res = BrowserExtract("https://example.com", "Estrai il titolo della pagina", "{""title"":""string""}", 9222)
  PRINT "EXTRACT="; res
  PRINT "VERIFICA="; INSTR(res, "Example Domain") > 0
END SUB
