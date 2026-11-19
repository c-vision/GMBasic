IMPORT AI WITH ALIAS
IMPORT NET
IMPORT HTTP
IMPORT STRINGS
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY

DIM cDpResult AS STRING
DIM cDpReady AS BIT
DIM cDpFetchEnabled AS INTEGER
DIM cDpFetchHeaders AS STRING

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

SUB BrowserWait(ms AS INTEGER)
  SLEEP ms
END SUB

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

FUNCTION BrowserTitle(port AS INTEGER) AS STRING
  DIM ws AS WEBSOCKET
  WEBSOCKET CONNECT "127.0.0.1", port, ws, CdpPath(port), 8000
  ws.OnMessage = "CdpHandler"
  cDpReady = 0
  ws.Send("{""id"":2,""method"":""Runtime.evaluate"",""params"":{""expression"":""document.title"",""returnByValue"":true}}")
  DO WHILE NOT cDpReady
    SLEEP 50
  LOOP
  BrowserTitle = cDpResult
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

' Handler per l'intercettazione Fetch (D3): a ogni Fetch.requestPaused risponde con
' Fetch.continueRequest includendo cDpFetchHeaders — la richiesta riprende con gli header.
SUB CdpFetchHandler(ws AS WEBSOCKET, msg$)
  DIM rid AS STRING
  DIM p1 AS INTEGER
  DIM needle AS STRING
  IF INSTR(msg$, CHR$(34) + "id" + CHR$(34) + ":30") > 0 THEN
    cDpResult = msg$
    cDpReady = 1
    EXIT SUB
  END IF
  IF cDpFetchEnabled AND INSTR(msg$, "requestPaused") > 0 THEN
    needle = CHR$(34) + "requestId" + CHR$(34) + ":" + CHR$(34)
    p1 = INSTR(msg$, needle)
    IF p1 > 0 THEN
      p1 = p1 + LEN(needle)
      rid = MID$(msg$, p1, INSTR(MID$(msg$, p1, LEN(msg$) - p1 + 1), CHR$(34)) - 1)
      ws.Send("{""id"":40,""method"":""Fetch.continueRequest"",""params"":{""requestId"":""" + rid + CHR$(34) + ",""headers"":" + cDpFetchHeaders + "}}")
    END IF
  END IF
END SUB

' C7 — estrazione STRUTTURATA: estrae il valore stringa del campo key$ dal JSON CDP.
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

' C7 — estrazione STRUTTURATA (BrowserExtract): naviga la pagina, ne legge il TESTO e chiede
' all'agente di estrarre i dati come da SCHEMA (JSON {"campo":"tipo"}), con validazione.
' Ritorna il JSON strutturato (o un errore esplicito).
FUNCTION BrowserExtract(url AS STRING, instruction AS STRING, schema AS STRING, port AS INTEGER) AS STRING
  DIM raw AS STRING
  DIM text AS STRING
  BrowserNavigate(url, port)
  SLEEP 600
  raw = BrowserText(port)
  text = CdpJsonStringValue(raw, "value")
  BrowserExtract = AI.AGENTASKSYNC("extract", "EXTRACT STRUCTURED SCHEMA: " + schema + " INSTRUCTION: " + instruction + " PAGE: " + text)
END FUNCTION

' ── D3 — autenticazione/rete via CDP Network.* ────────────────────────────────
' Comando CDP generico (connect + send + attesa risposta) — riusa i globals cDpResult/cDpReady.
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

' Imposta header HTTP extra per TUTTE le richieste (Network.setExtraHTTPHeaders).
' headersJson deve essere un JSON oggetto: {"X-Custom":"v","Authorization":"..."}.
FUNCTION BrowserSetHeaders(headersJson AS STRING, port AS INTEGER) AS STRING
  BrowserSetHeaders = CdpCommand("{""id"":21,""method"":""Network.setExtraHTTPHeaders"",""params"":{""headers"":" + headersJson + "}}", port)
END FUNCTION

' Override dello User-Agent (Network.setUserAgentOverride).
FUNCTION BrowserSetUserAgent(ua AS STRING, port AS INTEGER) AS STRING
  BrowserSetUserAgent = CdpCommand("{""id"":22,""method"":""Network.setUserAgentOverride"",""params"":{""userAgent"":" + CHR$(34) + ua + CHR$(34) + "}}", port)
END FUNCTION

' Imposta un cookie (Network.setCookie) — url opzionale per il domain.
FUNCTION BrowserSetCookie(name AS STRING, value AS STRING, url AS STRING, port AS INTEGER) AS STRING
  DIM u AS STRING
  u = url
  IF LEN(u) = 0 THEN u = "http://127.0.0.1"
  BrowserSetCookie = CdpCommand("{""id"":23,""method"":""Network.setCookie"",""params"":{""name"":" + CHR$(34) + name + CHR$(34) + ",""value"":" + CHR$(34) + value + CHR$(34) + ",""url"":" + CHR$(34) + u + CHR$(34) + "}}", port)
END FUNCTION

' Legge tutti i cookie (Network.getAllCookies) — ritorna il JSON raw della risposta.
FUNCTION BrowserGetCookies(port AS INTEGER) AS STRING
  BrowserGetCookies = CdpCommand("{""id"":24,""method"":""Network.getAllCookies""}", port)
END FUNCTION

' Basic Auth (Network.setExtraHTTPHeaders con Authorization: Basic base64(user:pass)).
FUNCTION BrowserAuth(user AS STRING, pass AS STRING, port AS INTEGER) AS STRING
  DIM token AS STRING
  token = TOBASE64(user + ":" + pass)
  BrowserAuth = BrowserSetHeaders("{""Authorization"":" + CHR$(34) + "Basic " + token + CHR$(34) + "}", port)
END FUNCTION

' D3 (fetch) — intercettazione richieste (Fetch.enable + continueRequest): applica gli header
' custom / basic auth in modo AFFIDABILE (Network.setExtraHTTPHeaders non li applica su Chrome
' 149 headless). headersList = ARRAY [{name,value},...] (il formato di Fetch.continueRequest).
' La connessione resta aperta: CdpFetchHandler risponde a ogni Fetch.requestPaused.
FUNCTION BrowserSetFetchHeaders(headersList AS STRING, port AS INTEGER) AS STRING
  DIM ws AS WEBSOCKET
  DIM guard AS INTEGER
  cDpFetchHeaders = headersList
  cDpFetchEnabled = 1
  WEBSOCKET CONNECT "127.0.0.1", port, ws, CdpPath(port), 8000
  IF ws IS NOTHING THEN
    BrowserSetFetchHeaders = "ERROR: CDP non raggiungibile"
    EXIT FUNCTION
  END IF
  ws.OnMessage = "CdpFetchHandler"
  cDpReady = 0
  ws.Send("{""id"":30,""method"":""Fetch.enable"",""params"":{""patterns"":[{""urlPattern"":""*""}]}}")
  guard = 0
  DO WHILE NOT cDpReady AND guard < 200
    SLEEP 50
    guard = guard + 1
  LOOP
  BrowserSetFetchHeaders = cDpResult
END FUNCTION


' ── helper C1 — trova la parentesi graffa di apertura dell'oggetto pagina nel /json ──
FUNCTION CdpLastOpenBrace(raw AS STRING, beforePos AS INTEGER) AS INTEGER
  DIM p AS INTEGER
  p = beforePos
  DO
    p = p - 1
    IF p < 1 THEN
      EXIT DO
    END IF
    IF MID$(raw, p, 1) = "{" THEN
      CdpLastOpenBrace = p
      EXIT FUNCTION
    END IF
  LOOP
  CdpLastOpenBrace = 0
END FUNCTION

' ── C1 — selezione pagina/tab dal /json ───────────────────────────────────────
' BrowserConnect(port, pageIndexOrUrl): ritorna il path WebSocket della pagina scelta.
'   - vuoto        → prima pagina (compatibile con CdpPath).
'   - "0","1",...  → pagina per INDICE (ordine del /json).
'   - "frameId"    → la pagina il cui URL o titolo contiene la stringa.
FUNCTION BrowserConnect(port AS INTEGER, pageIndexOrUrl AS STRING) AS STRING
  DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:" + LTRIM(STR$(port)) + "/json")
  DIM j$ AS STRING
  DIM wantIndex AS INTEGER
  DIM isIndex AS BIT
  DIM idx AS INTEGER
  DIM p1 AS INTEGER
  DIM u AS STRING
  DIM t AS STRING
  DIM foundPath AS STRING
  j$ = r.Body
  wantIndex = VAL(pageIndexOrUrl)
  IF LEN(pageIndexOrUrl) > 0 THEN
    IF pageIndexOrUrl = LTRIM(STR$(wantIndex)) THEN
      isIndex = 1
    END IF
  END IF
  IF isIndex THEN
    idx = 0
    p1 = 1
    DO
      p1 = INSTR(MID$(j$, p1, LEN(j$) - p1 + 1), """url"":""")
      IF p1 = 0 THEN
        EXIT DO
      END IF
      IF idx = wantIndex THEN
        foundPath = CdpJsonStringValue(MID$(j$, p1, LEN(j$) - p1 + 1), "webSocketDebuggerUrl")
        EXIT DO
      END IF
      idx = idx + 1
      p1 = p1 + 1
    LOOP
  END IF
  IF NOT isIndex THEN
    p1 = 1
    DO
      p1 = INSTR(MID$(j$, p1, LEN(j$) - p1 + 1), """url"":""")
      IF p1 = 0 THEN
        EXIT DO
      END IF
      u = CdpJsonStringValue(MID$(j$, p1, LEN(j$) - p1 + 1), "url")
      IF INSTR(u, pageIndexOrUrl) > 0 THEN
        foundPath = CdpJsonStringValue(MID$(j$, p1, LEN(j$) - p1 + 1), "webSocketDebuggerUrl")
        EXIT DO
      END IF
      t = CdpJsonStringValue(MID$(j$, CdpLastOpenBrace(j$, p1), LEN(j$) - CdpLastOpenBrace(j$, p1) + 1), "title")
      IF INSTR(t, pageIndexOrUrl) > 0 THEN
        foundPath = CdpJsonStringValue(MID$(j$, p1, LEN(j$) - p1 + 1), "webSocketDebuggerUrl")
        EXIT DO
      END IF
      p1 = p1 + 1
    LOOP
  END IF
  IF LEN(foundPath) = 0 THEN
    BrowserConnect = CdpPath(port)
  ELSE
    BrowserConnect = foundPath
  END IF
END FUNCTION


' ── C2 — configurazione (timeout comando CDP) ────────────────────────────────
DIM cDpCmdTimeout AS INTEGER
cDpCmdTimeout = 200   ' iterazioni × 50ms = 10s default

FUNCTION BrowserSetTimeout(port AS INTEGER, commandTimeoutMs AS INTEGER) AS STRING
  cDpCmdTimeout = commandTimeoutMs / 50
  IF cDpCmdTimeout < 2 THEN cDpCmdTimeout = 2
  BrowserSetTimeout = "ok"
END FUNCTION

' ── C3 — attesa per condizione (selettore o testo) ───────────────────────────
' BrowserWaitFor(port, selectorOrText, timeoutMs): polla la pagina finché il selettore
' esiste o il testo appare (document.querySelector / body.innerText.includes). Ritorna
' "found" o "" al timeout.
FUNCTION BrowserWaitFor(port AS INTEGER, selectorOrText AS STRING, timeoutMs AS INTEGER) AS STRING
  DIM deadline AS INTEGER
  DIM found AS BIT
  DIM js AS STRING
  DIM raw AS STRING
  DIM guard AS INTEGER
  deadline = timeoutMs / 50
  IF deadline < 2 THEN deadline = 2
  js = "!!document.querySelector('" + selectorOrText + "') || document.body.innerText.includes('" + selectorOrText + "')"
  found = 0
  guard = 0
  DO WHILE NOT found AND guard < deadline
    raw = CdpCommand("{""id"":50,""method"":""Runtime.evaluate"",""params"":{""expression"":" + CHR$(34) + js + CHR$(34) + ",""returnByValue"":true}}", port)
    found = 0
    IF INSTR(raw, CHR$(34) + "value" + CHR$(34) + ":true") > 0 THEN found = 1
    IF NOT found THEN SLEEP 50
    guard = guard + 1
  LOOP
  IF found THEN BrowserWaitFor = "found" ELSE BrowserWaitFor = ""
END FUNCTION

' ── C4 — interazione avanzata (click / fill / scroll) via Runtime.evaluate ────
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

FUNCTION BrowserScroll(port AS INTEGER, dx AS INTEGER, dy AS INTEGER) AS STRING
  DIM js AS STRING
  js = "window.scrollBy(" + LTRIM(STR$(dx)) + "," + LTRIM(STR$(dy)) + "); window.scrollY"
  BrowserScroll = CdpCommand("{""id"":62,""method"":""Runtime.evaluate"",""params"":{""expression"":" + CHR$(34) + js + CHR$(34) + ",""returnByValue"":true}}", port)
END FUNCTION




' ── C5 — gestione errori CDP ──────────────────────────────────────────────────
' BrowserCdpError(raw): se il risultato CDP contiene un errore ("error" del
' protocollo O "exceptionDetails" di Runtime.evaluate), ritorna la descrizione;
' altrimenti stringa vuota. MAI un fallback silenzioso.
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

' ── C6 — download di un file (Browser.setDownloadBehavior) ───────────────────
' BrowserDownload(port, url, destDir): abilita il download verso destDir e naviga
' alla URL (se è un file il Chromium lo scarica lì senza il dialog). Ritorna il
' JSON della risposta o un errore esplicito.
FUNCTION BrowserDownload(port AS INTEGER, url AS STRING, destDir AS STRING) AS STRING
  DIM raw AS STRING
  DIM err AS STRING
  raw = CdpCommand("{""id"":70,""method"":""Browser.setDownloadBehavior"",""params"":{""behavior"":""allow"",""downloadPath"":" + CHR$(34) + destDir + CHR$(34) + "}}", port)
  err = BrowserCdpError(raw)
  IF LEN(err) > 0 THEN
    BrowserDownload = "ERROR: " + err
    EXIT FUNCTION
  END IF
  raw = BrowserNavigate(url, port)
  err = BrowserCdpError(raw)
  IF LEN(err) > 0 THEN
    BrowserDownload = "ERROR: " + err
    EXIT FUNCTION
  END IF
  BrowserDownload = "ok"
END FUNCTION


REM ── main di test (C5-C6) ────────────────────────────────────────────────────
DIM port AS INTEGER = 9222
DIM raw AS STRING
DIM err AS STRING
DIM ok$ AS STRING

PRINT "BrowserC5C6: avvio"

raw = BrowserEvaluate("document.querySelector('#nonexistent').innerText", port)
err = BrowserCdpError(raw)
PRINT "C5 js-error desc: "; err

raw = CdpCommand("{""id"":99,""method"":""NoSuchMethod.xyz"",""params"":{}}", port)
err = BrowserCdpError(raw)
PRINT "C5 proto-error: "; err

ok$ = BrowserDownload(port, "http://127.0.0.1:8103/dl.txt", "work")
PRINT "C6 download: "; ok$
SLEEP 1500
PRINT "C6 file present (atteso -1): "; IO.FileExists("work/dl.txt")

PRINT "BrowserC5C6: fine"
