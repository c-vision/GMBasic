' browser.bas — capacita' CDP del plugin browser (Chromium headless).
' Espone le procedure del manifest (BrowserNavigate/Text/Title/Evaluate/Screenshot/Wait).
' Protocollo: Chrome DevTools Protocol via WebSocket (WEBSOCKET CONNECT + ws.Send). La
' risposta di ogni comando CDP arriva sulla callback ws.OnMessage e viene conservata in
' cDpResult$ (il JSON completo; il programma la parsa con JSON.PARSE).
'
' PREREQUISITI: Chromium avviato con --remote-debugging-port (helper launch_chromium.sh),
' porta 9222 default. Il path del WebSocket CDP si RICAVA dal /json (webSocketDebuggerUrl,
' id pagina reale — /devtools/page/1 fisso NON risponde). Le variabili top-level sono
' globali (DIM, non DIM SHARED — non supportato a livello top-level).

IMPORT AI WITH ALIAS
IMPORT NET
IMPORT HTTP
IMPORT STRINGS
IMPORT CRYPTOGRAPHY
IMPORT CONVERT

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
  DIM rel AS INTEGER
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
      rel = INSTR(MID$(j$, p1, LEN(j$) - p1 + 1), CHR$(34) + "url" + CHR$(34) + ":")
      IF rel = 0 THEN
        EXIT DO
      END IF
      p1 = p1 + rel - 1
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
      rel = INSTR(MID$(j$, p1, LEN(j$) - p1 + 1), CHR$(34) + "url" + CHR$(34) + ":")
      IF rel = 0 THEN
        EXIT DO
      END IF
      p1 = p1 + rel - 1
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

' ── D1 — loop computer-use: screenshot + vision → decisione ───────────────────
' BrowserAnalyze(port, prompt, outPath): cattura lo screenshot in outPath e chiede al
' modello VISION di decidere il prossimo step (prompt 'NEXT STEP'). Ritorna la decisione.
FUNCTION BrowserAnalyze(port AS INTEGER, prompt AS STRING, outPath AS STRING) AS STRING
  DIM raw AS STRING
  DIM b64 AS STRING
  DIM err AS STRING
  raw = BrowserScreenshot(outPath, port)
  b64 = CdpJsonStringValue(raw, "data")
  IF LEN(b64) = 0 THEN
    err = BrowserCdpError(raw)
    IF LEN(err) > 0 THEN
      BrowserAnalyze = "ERROR: screenshot non riuscito (" + err + ")"
    ELSE
      BrowserAnalyze = "ERROR: screenshot non riuscito (nessun dato)"
    END IF
    EXIT FUNCTION
  END IF
  OPEN outPath FOR BINARY AS #1
  PUT #1, , FROMBASE64(b64)
  CLOSE #1
  BrowserAnalyze = AI.ANALYZEIMAGE(outPath, prompt)
END FUNCTION

' ── D2 — sessione multi-step: back/forward, tab, stato + memoria ─────────────
DIM cDpSessionLog AS STRING
DIM cDpStepCount AS INTEGER

FUNCTION BrowserBack(port AS INTEGER) AS STRING
  BrowserBack = BrowserEvaluate("history.back()", port)
END FUNCTION

FUNCTION BrowserForward(port AS INTEGER) AS STRING
  BrowserForward = BrowserEvaluate("history.forward()", port)
END FUNCTION

FUNCTION BrowserState(port AS INTEGER) AS STRING
  DIM js AS STRING
  js = "JSON.stringify({u:location.href,t:document.title,l:history.length})"
  BrowserState = BrowserEvaluate(js, port)
END FUNCTION

' BrowserTabs(port): elenca le pagine aperte dal /json — una riga per tab:
' "<indice>|<url>|<titolo>" (il titolo può essere vuoto). "" se non ci sono pagine.
FUNCTION BrowserTabs(port AS INTEGER) AS STRING
  DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:" + LTRIM(STR$(port)) + "/json")
  DIM j$ AS STRING
  DIM p1 AS INTEGER
  DIM pageStart AS INTEGER
  DIM seg AS STRING
  DIM u AS STRING
  DIM t AS STRING
  DIM idx AS INTEGER
  DIM out$ AS STRING
  j$ = r.Body
  idx = 0
  p1 = 1
  out$ = ""
  DO
    rel = INSTR(MID$(j$, p1, LEN(j$) - p1 + 1), CHR$(34) + "url" + CHR$(34) + ":")
    IF rel = 0 THEN
      EXIT DO
    END IF
    p1 = p1 + rel - 1
    pageStart = CdpLastOpenBrace(j$, p1)
    seg = MID$(j$, pageStart, LEN(j$) - pageStart + 1)
    u = CdpJsonStringValue(seg, "url")
    t = CdpJsonStringValue(seg, "title")
    out$ = out$ + LTRIM(STR$(idx)) + "|" + u + "|" + t + CHR$(10)
    idx = idx + 1
    p1 = p1 + 1
  LOOP
  BrowserTabs = out$
END FUNCTION

' BrowserSessionRecord(port, action, detail): memorizza un passo della sessione
' (memoria dell'agente su ciò che ha fatto/visto). BrowserSessionLog(port) la restituisce.
FUNCTION BrowserSessionRecord(port AS INTEGER, action AS STRING, detail AS STRING) AS STRING
  cDpSessionLog = cDpSessionLog + action + "|" + detail + CHR$(10)
  BrowserSessionRecord = "ok"
END FUNCTION

FUNCTION BrowserSessionLog(port AS INTEGER) AS STRING
  BrowserSessionLog = cDpSessionLog
END FUNCTION

' ── D4 — download PDF + estrazione testo (ingestion → RAG) ────────────────────
' BrowserDownloadPdf(port, url, destDir, fileName): scarica la URL (un PDF) in
' destDir/fileName e ne estrae il TESTO via CONVERT.ToText (gmb_doc_pdf.c).
' Ritorna il testo estratto, o un errore esplicito. Il chiamante lo indicizza (RAG).
FUNCTION BrowserDownloadPdf(port AS INTEGER, url AS STRING, destDir AS STRING, fileName AS STRING) AS STRING
  DIM r AS FETCHRESULT = FETCH(url)
  DIM b64 AS STRING
  DIM path AS STRING
  DIM text AS STRING
  b64 = r.Body
  IF LEN(b64) = 0 THEN
    BrowserDownloadPdf = "ERROR: download fallito da " + url
    EXIT FUNCTION
  END IF
  path = destDir + "/" + fileName
  OPEN path FOR BINARY AS #1
  PUT #1, , FROMBASE64(b64)
  CLOSE #1
  text = CONVERT.ToText(path)
  IF LEN(text) = 0 THEN
    BrowserDownloadPdf = "ERROR: estrazione testo fallita da " + path
    EXIT FUNCTION
  END IF
  BrowserDownloadPdf = text
END FUNCTION

' ── E2 — sessione persistente (cookie con expiry nel profilo stabile) ─────────
' BrowserSetCookiePersistent(name, value, url, port): come BrowserSetCookie ma con
' 'expires' lontano (2036) → il cookie sopravvive alla chiusura del browser e resta nel
' profilo stabile (~/.gmbasic/chromium/profile) tra le run (login una volta).
FUNCTION BrowserSetCookiePersistent(name AS STRING, value AS STRING, url AS STRING, port AS INTEGER) AS STRING
  DIM u AS STRING
  u = url
  IF LEN(u) = 0 THEN u = "http://127.0.0.1"
  BrowserSetCookiePersistent = CdpCommand("{""id"":25,""method"":""Network.setCookie"",""params"":{""name"":" + CHR$(34) + name + CHR$(34) + ",""value"":" + CHR$(34) + value + CHR$(34) + ",""url"":" + CHR$(34) + u + CHR$(34) + ",""expires"":2100000000}}", port)
END FUNCTION

' ── E3 — Web→RAG diretta: naviga, chunk, indicizza nel vector DB ──────────────
' BrowserToRag(url, db, coll, docId, pattern, port): naviga la pagina, ne estrae il
' testo visibile, lo spezza in chunk col PATTERN (SPLIT) e indicizza OGNI chunk nel
' vector DB (AiVectorRuntime) con id docId-N. Richiede il DB/collection già pronti.
' Ritorna "ok <n> chunk" o un errore esplicito.
FUNCTION BrowserToRag(url AS STRING, db AS STRING, coll AS STRING, docId AS STRING, pattern AS STRING, port AS INTEGER) AS STRING
  DIM raw AS STRING
  DIM text AS STRING
  DIM chunk$(100)
  DIM i AS INTEGER
  DIM id AS STRING
  DIM done AS INTEGER
  BrowserNavigate(url, port)
  SLEEP 800
  raw = BrowserText(port)
  text = CdpJsonStringValue(raw, "value")
  IF LEN(text) = 0 THEN
    BrowserToRag = "ERROR: testo pagina vuoto"
    EXIT FUNCTION
  END IF
  chunk$() = SPLIT(text, pattern)
  done = 0
  FOR i = 0 TO UBOUND(chunk$)
    IF LEN(chunk$(i)) > 0 THEN
      id = docId + "-" + LTRIM(STR$(i))
      AI VECTOR INDEX TEXT db, coll, id, chunk$(i)
      done = done + 1
    END IF
  NEXT i
  BrowserToRag = "ok " + LTRIM(STR$(done)) + " chunk"
END FUNCTION

' ── E11 — warm start / processo condiviso ─────────────────────────────────────
' BrowserPing(port): verifica se il CDP è già attivo (riuso del processo). "ok" se il
' browser risponde, "" altrimenti.
FUNCTION BrowserPing(port AS INTEGER) AS STRING
  DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:" + LTRIM(STR$(port)) + "/json/version")
  IF LEN(r.Body) > 0 THEN
    BrowserPing = "ok"
  ELSE
    BrowserPing = ""
  END IF
END FUNCTION

' ── E8 — Visual QA: asserzione visiva via screenshot + vision ─────────────────
' BrowserAssert(port, assertion, outPath): cattura lo screenshot in outPath e chiede al
' modello VISION di ASSERIRE lo stato ("il pulsante è visibile? il layout è corretto?").
' Ritorna "true"/"false" (verdetto) o un errore esplicito.
FUNCTION BrowserAssert(port AS INTEGER, assertion AS STRING, outPath AS STRING) AS STRING
  DIM shot AS STRING
  DIM verdict AS STRING
  shot = BrowserAnalyze(port, "ASSERT: " + assertion, outPath)
  verdict = LTRIM(shot)
  BrowserAssert = verdict
END FUNCTION

' ── E9 — Human-like actions (delay non uniformi, mouse a curve, typing naturale) ──
' HumanDelay(minMs, maxMs): pausa pseudo-casuale tra min e max (parte del delay non
' uniforme). Ritorna la durata attesa in ms.
FUNCTION HumanDelay(minMs AS INTEGER, maxMs AS INTEGER) AS INTEGER
  DIM span AS INTEGER
  DIM d AS INTEGER
  span = maxMs - minMs
  IF span < 1 THEN span = 1
  d = minMs + RANDOMINT(0, span)
  SLEEP d
  HumanDelay = d
END FUNCTION

' HumanClick(port, selector): sposta il mouse a CURVE (passi interpolati su
' Input.dispatchMouseEvent) verso il centro dell'elemento + click. Mai teletrasporto.
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

' HumanType(port, selector, text): focus il campo e digita carattere per carattere
' (Input.insertText) con pause variabili tra i tasti.
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

' HumanScroll(port, dy, steps): scroll incrementale con pause (mai un salto unico).
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

' ── E10 — profilo coerente + anti-fingerprint ─────────────────────────────────
' BrowserProfile(port, ua, width, height): imposta un profilo COERENTE — User-Agent con
' sec-ch-ua allineati, viewport/device, accettazione lingua e rimozione di
' navigator.webdriver (marker headless) via script su ogni nuovo documento.
' Fingerprint stabile nella sessione.
FUNCTION BrowserProfile(port AS INTEGER, ua AS STRING, width AS INTEGER, height AS INTEGER) AS STRING
  DIM raw AS STRING
  raw = CdpCommand("{""id"":95,""method"":""Network.setUserAgentOverride"",""params"":{""userAgent"":" + CHR$(34) + ua + CHR$(34) + ",""acceptLanguage"":""it-IT,it;q=0.9"",""userAgentMetadata"":{""brands"":[{""brand"":""Chromium"",""version"":""149""},{""brand"":""Not(A:Brand"",""version"":""8""}],""fullVersion"":""149.0.7827.55"",""platform"":""macOS"",""platformVersion"":""15.0"",""architecture"":""arm64"",""model"":"""",""mobile"":false,""bitness"":""64""}}}", port)
  raw = CdpCommand("{""id"":96,""method"":""Emulation.setDeviceMetricsOverride"",""params"":{""width"":" + LTRIM(STR$(width)) + ",""height"":" + LTRIM(STR$(height)) + ",""deviceScaleFactor"":1,""mobile"":false}}", port)
  raw = CdpCommand("{""id"":97,""method"":""Page.addScriptToEvaluateOnNewDocument"",""params"":{""source"":""Object.defineProperty(navigator,'webdriver',{get:()=>undefined})""}}", port)
  BrowserProfile = "ok"
END FUNCTION

' ── E13 — intercettazione richieste: blocca URL + header personalizzati ───────
' BrowserBlockUrls(port, patternsJson): Network.setBlockedURLs — blocca le richieste che
' matchano i pattern (ads/analytics per coerenza e velocità). patternsJson = array JSON
' es. '["*://*/ad.js","*google-analytics*"]'.
FUNCTION BrowserBlockUrls(port AS INTEGER, patternsJson AS STRING) AS STRING
  DIM raw AS STRING
  raw = CdpCommand("{""id"":100,""method"":""Network.setBlockedURLs"",""params"":{""urls"":" + patternsJson + "}}", port)
  IF INSTR(raw, """error""") > 0 THEN
    BrowserBlockUrls = "ERROR: " + CdpJsonStringValue(raw, "message")
  ELSE
    BrowserBlockUrls = "ok"
  END IF
END FUNCTION

' ── E16 — etica/cortesia: robots.txt, rate-limit, avviso CAPTCHA ──────────────
' BrowserRobotsAllowed(url): recupera il robots.txt del sito e verifica se la URL è
' consentita (Disallow). Ritorna "allowed" o "blocked: <path>". Base: robot rispetta.
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

' BrowserRateLimit(minMs): pausa minima tra le richieste (cortesia).
FUNCTION BrowserRateLimit(minMs AS INTEGER) AS STRING
  SLEEP minMs
  BrowserRateLimit = "ok"
END FUNCTION

' BrowserCheckCaptcha(port): cerca i marker CAPTCHA nel testo della pagina
' ("captcha"/"recaptcha"/"verify you are human"). Ritorna "captcha" o "".
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

' ── E17 — deterministic replay per test (registra DOM + interazioni, riproduce) ──
' BrowserReplay(port, log): riproduce le azioni registrate (una per riga "action|detail"):
'   nav|url  -> BrowserNavigate    click|selector -> BrowserClick
'   type|selector|text -> BrowserFill
' Ritorna "replayed <n> steps". Base per i test E2E deterministici.
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

' ── E14 — memoria del piano (task state): salva su file, riprende dopo riavvio ──
' BrowserTaskSave(port, filePath): salva l'URL corrente + il session log (pagine visitate,
' dati estratti, prossimi passi) su file. BrowserTaskResume riprende da dove si e' fermato.
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

' ── E12 — osservabilità: sessione con PROVA VISIVA a ogni step, esportabile ──
' BrowserAuditStep(port, action, detail, shotDir): registra il passo NEL session log CON
' lo screenshot a ogni step (prova visiva) salvato in shotDir/step-N.png. Ritorna il path.
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

' BrowserAuditExport(port, outDir): esporta il session log (passi in ordine, con le prove
' visive) in outDir/audit.log. Il chiamante può montarlo come audit sink.
FUNCTION BrowserAuditExport(port AS INTEGER, outDir AS STRING) AS STRING
  DIM log AS STRING
  log = cDpSessionLog
  OPEN outDir + "/audit.log" FOR OUTPUT AS #1
  PRINT #1, log
  CLOSE #1
  BrowserAuditExport = log
END FUNCTION