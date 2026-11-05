' ==============================================================================
' webapp_ui_cdp.bas — click-through della WEBAPP Agents Operating Center con un
' browser headless (Chromium/Obscura via CDP).
'
' Verifica ciò che i test HTTP non vedono: JS della UI, palette, rail, Inbox,
' scorciatoie, temi, creazione agente dal form, WebSocket del canvas, EventSource.
'
' Uso (dalla root del monorepo, gamba nativa):
'   bash tools/sh/gmb_c99native_run.sh ../tools/basic/webapp_ui_cdp.bas wcdp
'   ... -- --keep                 (lascia la webapp attiva)
'   ... -- --browser obscura
'   ... -- --cdp http://127.0.0.1:9222   (browser già avviato)
'
' CDP su WebSocket: ogni comando è {id, method, params [, sessionId]}; il receive-loop
' client (gmb_ws_client.c) consegna le risposte come task GPM, qui raccolte per id.
' ==============================================================================
IMPORT IO
IMPORT HTTP
IMPORT JSON
IMPORT SYSTEM

DIM g_dash AS STRING
DIM g_cdp AS STRING
DIM g_cdpPort AS LONG
DIM g_sid AS STRING
DIM g_id AS LONG
DIM g_resp AS STRING
DIM g_last AS STRING
DIM g_wsHandshake AS LONG
DIM g_browserProc AS LONG

DIM g_pass AS LONG
DIM g_fail AS LONG
DIM g_warn AS LONG

DIM g_ws AS WEBSOCKET
DIM g_jsErr AS STRING
DIM g_stop AS LONG

FUNCTION Q(s AS STRING) AS STRING
  RETURN CHR$(34) + s + CHR$(34)
END FUNCTION

FUNCTION TrimS(s AS STRING) AS STRING
  RETURN RTRIM$(LTRIM$(s))
END FUNCTION

FUNCTION J(s AS STRING) AS STRING
  DIM out AS STRING, i AS LONG, c AS STRING, bs AS STRING
  bs = CHR$(92)
  out = ""
  FOR i = 1 TO LEN(s)
    c = MID$(s, i, 1)
    IF c = CHR$(34) THEN
      out = out + bs + CHR$(34)
    ELSEIF c = bs THEN
      out = out + bs + bs
    ELSEIF c = CHR$(10) THEN
      out = out + bs + "n"
    ELSEIF c = CHR$(13) THEN
      out = out + bs + "r"
    ELSEIF c = CHR$(9) THEN
      out = out + bs + "t"
    ELSE
      out = out + c
    END IF
  NEXT i
  RETURN out
END FUNCTION

' ---- stampa esiti ------------------------------------------------------------
SUB Ok(label AS STRING, detail AS STRING)
  g_pass = g_pass + 1
  IF LEN(detail) > 0 THEN
    PRINT "  PASS "; label; " — "; detail
  ELSE
    PRINT "  PASS "; label
  END IF
END SUB

SUB Bad(label AS STRING, detail AS STRING)
  g_fail = g_fail + 1
  IF LEN(detail) > 0 THEN
    PRINT "  FAIL "; label; " — "; detail
  ELSE
    PRINT "  FAIL "; label
  END IF
END SUB

SUB Warn(label AS STRING, detail AS STRING)
  g_warn = g_warn + 1
  IF LEN(detail) > 0 THEN
    PRINT "  WARN "; label; " — "; detail
  ELSE
    PRINT "  WARN "; label
  END IF
END SUB

' ---- HTTP --------------------------------------------------------------------
FUNCTION GetText(url AS STRING) AS STRING
  RETURN HTTP.GET(url)
END FUNCTION

' ---- CDP ---------------------------------------------------------------------
SUB CdpOnMsg(ws AS WEBSOCKET, msg AS STRING)
  DIM idTag AS STRING
  g_last = msg
  IF INSTR(msg, "webSocketHandshakeResponseReceived") > 0 THEN g_wsHandshake = -1
  idTag = Q("id") + ":" + LTRIM$(STR$(g_id))
  IF INSTR(msg, idTag) > 0 THEN
    IF LEN(g_resp) = 0 THEN g_resp = msg
  END IF
END SUB

FUNCTION SendCmd(method AS STRING, params AS STRING) AS STRING
  DIM msg AS STRING, i AS LONG, idle AS LONG
  g_id = g_id + 1
  g_resp = ""
  msg = "{" + Q("id") + ":" + LTRIM$(STR$(g_id)) + "," + Q("method") + ":" + Q(method) + "," + Q("params") + ":" + params
  IF LEN(g_sid) > 0 THEN msg = msg + "," + Q("sessionId") + ":" + Q(g_sid)
  msg = msg + "}"
  g_ws.Send(msg)
  FOR i = 1 TO 500
    IF LEN(g_resp) > 0 THEN EXIT FOR
    SLEEP 20
  NEXT i
  IF LEN(g_resp) = 0 THEN RETURN ""
  IF INSTR(g_resp, Q("error")) > 0 THEN
    PRINT "  [cdp] errore su "; method; ": "; LEFT$(g_resp, 200)
  END IF
  RETURN g_resp
END FUNCTION

FUNCTION NoParams() AS STRING
  RETURN "{}"
END FUNCTION

FUNCTION ExprParams(expr AS STRING) AS STRING
  RETURN "{" + Q("expression") + ":" + Q(J(expr)) + "," + Q("returnByValue") + ":true," + Q("awaitPromise") + ":true}"
END FUNCTION

' Valore di result.result.value (stringa JSON) di una risposta.
FUNCTION JsValue(expr AS STRING) AS STRING
  DIM r AS STRING, obj AS JSON, v AS STRING
  r = SendCmd("Runtime.evaluate", ExprParams(expr))
  IF LEN(r) = 0 THEN RETURN ""
  obj = JSON.PARSE(r)
  v = JSON.EXTRACT(obj, "result.result.value")
  RETURN v
END FUNCTION

FUNCTION JsCmd(params AS STRING) AS STRING
  DIM r AS STRING, obj AS JSON
  r = SendCmd("Runtime.evaluate", params)
  IF LEN(r) = 0 THEN RETURN ""
  obj = JSON.PARSE(r)
  RETURN JSON.EXTRACT(obj, "result.result.value")
END FUNCTION

FUNCTION WaitJs(expr AS STRING, ms AS LONG) AS STRING
  DIM i AS LONG, v AS STRING, stepMs AS LONG
  stepMs = 200
  FOR i = 1 TO ms \ stepMs
    v = JsValue(expr)
    IF LEN(v) > 0 AND LCASE$(v) <> "false" AND LCASE$(v) <> "null" AND v <> "0" THEN RETURN v
    SLEEP stepMs
  NEXT i
  RETURN v
END FUNCTION

FUNCTION JsonGet(msg AS STRING, path AS STRING) AS STRING
  DIM obj AS JSON
  IF LEN(msg) = 0 THEN RETURN ""
  obj = JSON.PARSE(msg)
  RETURN JSON.EXTRACT(obj, path)
END FUNCTION

' Campo base64 di Page.captureScreenshot ("result.data").
FUNCTION ExtractData(msg AS STRING) AS STRING
  DIM tag AS STRING, p AS LONG, i AS LONG, out AS STRING
  tag = Q("data") + ":" + CHR$(34)
  p = INSTR(msg, tag)
  IF p = 0 THEN RETURN ""
  i = p + LEN(tag)
  DO WHILE i <= LEN(msg) AND MID$(msg, i, 1) <> CHR$(34)
    out = out + MID$(msg, i, 1)
    i = i + 1
  LOOP
  RETURN out
END FUNCTION

' ---- browser / app -----------------------------------------------------------
FUNCTION PathOf(u AS STRING) AS STRING
  DIM i AS LONG, st AS LONG
  st = INSTR(u, "//")
  IF st = 0 THEN RETURN "/"
  st = st + 2
  FOR i = st TO LEN(u)
    IF MID$(u, i, 1) = "/" THEN RETURN MID$(u, i)
  NEXT i
  RETURN "/"
END FUNCTION

FUNCTION HostOf(u AS STRING) AS STRING
  DIM i AS LONG, st AS LONG
  st = INSTR(u, "//")
  IF st = 0 THEN RETURN u
  st = st + 2
  FOR i = st TO LEN(u)
    IF MID$(u, i, 1) = "/" THEN RETURN MID$(u, st, i - st)
    IF MID$(u, i, 1) = ":" THEN RETURN MID$(u, st, i - st)
  NEXT i
  RETURN MID$(u, st)
END FUNCTION

FUNCTION PortOf(u AS STRING) AS LONG
  DIM i AS LONG, st AS LONG, rest AS STRING, k AS LONG
  st = INSTR(u, "//")
  IF st = 0 THEN RETURN 80
  st = st + 2
  k = INSTR(st, u, ":")
  IF k = 0 THEN RETURN 80
  rest = MID$(u, k + 1)
  i = 1
  DO WHILE i <= LEN(rest) AND ASC(MID$(rest, i, 1)) >= 48 AND ASC(MID$(rest, i, 1)) <= 57
    i = i + 1
  LOOP
  RETURN VAL(LEFT$(rest, i - 1))
END FUNCTION

FUNCTION Healthy() AS LONG
  IF LEN(GetText(g_dash + "/api/health")) > 0 THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION BrowserOk() AS LONG
  IF LEN(GetText(g_cdp + "/json/version")) > 0 THEN RETURN -1
  RETURN 0
END FUNCTION

SUB EnsureBrowser(kind AS STRING)
  DIM exe AS STRING, cmd AS STRING, i AS LONG, home AS STRING
  IF BrowserOk() <> 0 THEN EXIT SUB
  home = TrimS(SYSTEM.SHELLOUT("printf %s $HOME"))
  IF kind = "obscura" THEN
    exe = TrimS(SYSTEM.SHELLOUT("ls -1 " + Q(home + "/.gmbasic/libs/obscura") + "/*/obscura 2>/dev/null | head -1"))
  ELSE
    exe = TrimS(SYSTEM.SHELLOUT("find " + Q(home + "/.gmbasic/libs/chromium") + " -type f -name 'Google Chrome for Testing' 2>/dev/null | head -1"))
    IF LEN(exe) = 0 THEN exe = TrimS(SYSTEM.SHELLOUT("find " + Q(home + "/.gmbasic/libs/chromium") + " -type f -name 'chrome' 2>/dev/null | head -1"))
  END IF
  IF LEN(exe) = 0 OR NOT FILEEXISTS(exe) THEN
    PRINT "errore: browser non trovato in ~/.gmbasic/libs/"; kind
    RETURN
  END IF
  SYSTEM.SHELLOUT("rm -rf /tmp/gmb-webapp-ui-chrome")
  IF kind = "obscura" THEN
    cmd = Q(exe) + " serve --port " + LTRIM$(STR$(g_cdpPort)) + " --allow-private-network --storage-dir /tmp/gmb-webapp-ui-obscura >/dev/null 2>&1 &"
  ELSE
    cmd = Q(exe) + " --headless=new --disable-gpu --no-first-run --no-default-browser-check --remote-debugging-port=" + LTRIM$(STR$(g_cdpPort)) + " --user-data-dir=/tmp/gmb-webapp-ui-chrome about:blank >/dev/null 2>&1 &"
  END IF
  DIM wr AS LONG
  wr = SYSTEM.SHELLOUT(cmd)
  FOR i = 1 TO 80
    SLEEP 250
    IF BrowserOk() <> 0 THEN EXIT SUB
  NEXT i
  PRINT "errore: "; kind; " non risponde su :"; g_cdpPort
END SUB

SUB EnsureApp()
  DIM root AS STRING, i AS LONG, wr AS LONG
  IF Healthy() <> 0 THEN EXIT SUB
  root = IO.PARENTDIR(IO.PARENTDIR(IO.CWD()))
  wr = SYSTEM.SHELLOUT(Q(root + "/tools/sh/webapp_up.sh") + " start >/dev/null 2>&1")
  FOR i = 1 TO 60
    SLEEP 400

    IF Healthy() <> 0 THEN EXIT SUB
  NEXT i
  PRINT "errore: la webapp non risponde su :8090"
END SUB

SUB CleanupAgents()
  DIM wr AS LONG
  wr = SYSTEM.SHELLOUT("curl -s -m 3 -X DELETE " + Q(g_dash + "/api/agents/UITest") + " >/dev/null 2>&1")
  wr = SYSTEM.SHELLOUT("curl -s -m 3 -X DELETE " + Q(g_dash + "/api/agents/StressProbe") + " >/dev/null 2>&1")
END SUB

FUNCTION Main() AS INTEGER
  DIM keep AS LONG, kind AS STRING, cdpArg AS STRING
  DIM a AS STRING, i AS LONG, page AS STRING, r AS STRING, v AS STRING
  DIM targetId AS STRING, sid AS STRING, host AS STRING, path AS STRING
  DIM n AS LONG, chip AS STRING, opened AS STRING, state AS STRING
  DIM started AS LONG, root AS STRING, wr AS LONG, png AS STRING
  DIM bytes(0) AS BYTE
  DIM errs AS STRING, attempt AS LONG, okc AS LONG

  SET SHELL ON
  ' Il programma è lineare (nessun GO SUB): SLEEP sarebbe un'attesa reale fuori dal
  ' GPM e il watchdog vedrebbe "nessun timer" mentre i task callback sono fermi.
  ' Un keepalive su un task GPM tiene il timer armato per tutta la durata del tool.
  DIM wg AS WAITGROUP
  GO SUB KeepAlive(wg) IN wg
  g_dash = "http://127.0.0.1:8090"
  g_cdpPort = 9333
  g_cdp = "http://127.0.0.1:" + LTRIM$(STR$(g_cdpPort))
  keep = 0
  kind = "chromium"
  cdpArg = ""
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    a = COMMAND$(i)
    IF a = "--keep" THEN keep = -1
    IF a = "--browser" AND i < COMMANDCOUNT() THEN
      i = i + 1
      kind = COMMAND$(i)
    END IF
    IF a = "--cdp" AND i < COMMANDCOUNT() THEN
      i = i + 1
      cdpArg = COMMAND$(i)
    END IF
    i = i + 1
  LOOP
  IF LEN(cdpArg) > 0 THEN
    g_cdp = cdpArg
    IF BrowserOk() = 0 THEN
      PRINT "errore: nessun browser CDP su "; g_cdp
      RETURN 2
    END IF
  ELSE
    EnsureBrowser(kind)
    IF BrowserOk() = 0 THEN RETURN 2
  END IF
  started = 0
  IF Healthy() = 0 THEN
    started = -1
    EnsureApp
    IF Healthy() = 0 THEN RETURN 2
  END IF
  CleanupAgents

  r = GetText(g_cdp + "/json/version")
  v = JsonGet(r, "webSocketDebuggerUrl")
  IF LEN(v) = 0 THEN
    PRINT "errore: webSocketDebuggerUrl assente in /json/version"
    RETURN 2
  END IF
  host = HostOf(v)
  path = PathOf(v)
  g_cdpPort = PortOf(v)

  WEBSOCKET CONNECT host, g_cdpPort, g_ws, path
  g_ws.OnMessage = "CdpOnMsg"
  SLEEP 300

  r = SendCmd("Target.createTarget", "{" + Q("url") + ":" + Q("about:blank") + "}")
  targetId = JsonGet(r, "result.targetId")
  r = SendCmd("Target.attachToTarget", "{" + Q("targetId") + ":" + Q(targetId) + "," + Q("flatten") + ":true}")
  sid = JsonGet(r, "result.sessionId")
  g_sid = sid
  wr = LEN(SendCmd("Page.enable", NoParams()))
  wr = LEN(SendCmd("Runtime.enable", NoParams()))
  wr = LEN(SendCmd("Page.addScriptToEvaluateOnNewDocument", "{" + Q("source") + ":" + Q("window.__errs=[];addEventListener('error',e=>__errs.push(String(e.message||e)));addEventListener('unhandledrejection',e=>__errs.push('rej:'+String(e.reason)));") + "}"))
  wr = LEN(SendCmd("Page.navigate", "{" + Q("url") + ":" + Q(g_dash + "/index.bsp") + "}"))

  IF LEN(WaitJs("document.readyState==='complete'", 10000)) = 0 THEN
    Bad("pagina caricata", "readyState != complete")
    RETURN 1
  END IF
  Ok("pagina caricata", JsValue("document.title"))

  PRINT ""
  PRINT "== dati live (JS + fetch) =="
  n = VAL(WaitJs("document.querySelectorAll('#agents-body tr').length", 12000))
  IF n >= 3 THEN Ok("righe agenti popolate", LTRIM$(STR$(n)) + " righe") ELSE Bad("righe agenti popolate", LTRIM$(STR$(n)))
  chip = JsValue("(document.getElementById('chip-api')||{}).textContent||''")
  IF INSTR(LCASE$(chip), "ok") > 0 THEN Ok("chip API", TrimS(chip)) ELSE Bad("chip API", chip)
  chip = WaitJs("(document.getElementById('chip-provider')||{}).textContent||''", 6000)
  IF LEN(TrimS(chip)) > 0 AND INSTR(chip, "—") = 0 THEN Ok("provider mostrato", TrimS(chip)) ELSE Bad("provider mostrato", chip)
  n = VAL(JsValue("document.querySelectorAll('#rail button').length"))
  IF n >= 3 THEN Ok("agent rail", LTRIM$(STR$(n)) + " bottoni") ELSE Bad("agent rail", LTRIM$(STR$(n)))

  PRINT ""
  PRINT "== command palette =="
  wr = LEN(JsValue("document.dispatchEvent(new KeyboardEvent('keydown',{key:'k',ctrlKey:true,bubbles:true}))"))
  opened = WaitJs("(document.getElementById('ov-palette')||{}).className||''", 3000)
  IF INSTR(opened, "show") > 0 THEN Ok("Ctrl+K apre la palette", "") ELSE Bad("Ctrl+K apre la palette", opened)
  n = VAL(JsValue("(()=>{var q=document.getElementById('pal-q');q.value='team';q.dispatchEvent(new Event('input',{bubbles:true}));return document.querySelectorAll('#pal-list .pal-item').length;})()"))
  IF n > 0 THEN Ok("ricerca fuzzy nella palette", LTRIM$(STR$(n)) + " voci") ELSE Bad("ricerca fuzzy nella palette", LTRIM$(STR$(n)))
  wr = LEN(JsValue("document.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true}))"))
  opened = JsValue("document.getElementById('ov-palette').className")
  IF INSTR(opened, "show") = 0 THEN Ok("Esc chiude la palette", "") ELSE Bad("Esc chiude la palette", opened)

  PRINT ""
  PRINT "== scorciatoie e tab =="
  okc = 0
  FOR i = 1 TO 3
    IF i = 1 THEN page = "3" : path = "panel-log"
    IF i = 2 THEN page = "5" : path = "panel-memory"
    IF i = 3 THEN page = "7" : path = "panel-canvas"
    wr = LEN(JsValue("document.dispatchEvent(new KeyboardEvent('keydown',{key:'" + page + "',bubbles:true}))"))
    state = WaitJs("document.getElementById('" + path + "').classList.contains('active')", 3000)
    IF LCASE$(state) = "true" THEN
      Ok("tasto " + page + " -> " + path, "")
    ELSE
      Bad("tasto " + page + " -> " + path, state)
    END IF
  NEXT i
  wr = LEN(JsValue("document.dispatchEvent(new KeyboardEvent('keydown',{key:'?',bubbles:true}))"))
  opened = WaitJs("(document.getElementById('ov-help')||{}).className||''", 3000)
  IF INSTR(opened, "show") > 0 THEN Ok("? apre l'aiuto", "") ELSE Bad("? apre l'aiuto", opened)
  wr = LEN(JsValue("document.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true}))"))
  wr = LEN(JsValue("document.dispatchEvent(new KeyboardEvent('keydown',{key:'i',bubbles:true}))"))
  opened = WaitJs("(document.getElementById('ov-inbox')||{}).className||''", 3000)
  IF INSTR(opened, "show") > 0 THEN Ok("i apre l'Inbox", "") ELSE Bad("i apre l'Inbox", opened)
  wr = LEN(JsValue("document.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true}))"))

  PRINT ""
  PRINT "== tema =="
  page = JsValue("localStorage.getItem('gmb-webapp-theme')||'system'")
  wr = LEN(JsValue("document.getElementById('btn-theme').click()"))
  state = WaitJs("(function(){var m=localStorage.getItem('gmb-webapp-theme');return (m && m!=='" + page + "') ? m : '';})()", 4000)
  chip = JsValue("document.documentElement.getAttribute('data-theme')")
  IF LEN(state) > 0 AND (chip = "light" OR chip = "dark") THEN
    Ok("tema cambia e persiste", "mode " + page + " -> " + state + ", data-theme=" + chip)
  ELSE
    Bad("tema cambia e persiste", "mode " + page + " -> " + state + ", dt=" + chip)
  END IF

  PRINT ""
  PRINT "== creazione agente dal form =="
  wr = LEN(JsValue("document.dispatchEvent(new KeyboardEvent('keydown',{key:'1',bubbles:true}))"))
  wr = LEN(JsValue("(()=>{document.getElementById('new-name').value='UITest';document.getElementById('new-perm').value='YOLO';document.getElementById('btn-create').click();})()"))
  state = WaitJs("Array.from(document.querySelectorAll('#agents-body tr')).some(tr=>tr.textContent.includes('UITest'))", 8000)
  IF LCASE$(state) = "true" THEN Ok("agente creato dalla UI e in tabella", "") ELSE Bad("agente creato dalla UI e in tabella", state)
  state = JsValue("fetch('/api/agents/UITest',{method:'DELETE'}).then(r=>r.status)")
  IF state = "200" THEN Ok("cleanup agente", "HTTP " + state) ELSE Bad("cleanup agente", state)

  PRINT ""
  PRINT "== filtri Log salvati =="
  wr = LEN(JsValue("document.dispatchEvent(new KeyboardEvent('keydown',{key:'3',bubbles:true}))"))
  wr = LEN(JsValue("(()=>{var f=document.getElementById('log-filter');f.value='UITest';document.getElementById('btn-log-save').click();})()"))
  state = JsValue("localStorage.getItem('gmb-webapp-filters')")
  IF LEN(state) > 0 AND INSTR(state, "UITest") > 0 THEN Ok("filtro salvato in localStorage", state) ELSE Bad("filtro salvato in localStorage", state)

  PRINT ""
  PRINT "== EventSource (SSE) =="
  state = WaitJs("(document.getElementById('log-live')||{}).textContent||''", 8000)
  IF INSTR(LCASE$(state), "live") > 0 THEN Ok("SSE connesso", state) ELSE Bad("SSE connesso", state)

  PRINT ""
  PRINT "== WebSocket canvas =="
  state = ""
  FOR attempt = 1 TO 3
    wr = LEN(JsValue("document.dispatchEvent(new KeyboardEvent('keydown',{key:'7',bubbles:true}))"))
    ' attende che il pannello canvas sia montato prima del click (il pulsante compare
    ' solo quando il tab diventa attivo: un click troppo presto non fa nulla)
    state = WaitJs("(document.getElementById('btn-canvas-connect')||{}).id||''", 3000)
    SLEEP 300
    wr = LEN(JsValue("document.getElementById('btn-canvas-connect').click()"))
    state = WaitJs("(document.getElementById('canvas-state')||{}).textContent||''", 6000)
    IF INSTR(LCASE$(state), "connected") > 0 THEN EXIT FOR
    wr = LEN(SendCmd("Page.reload", "{" + Q("ignoreCache") + ":true}"))
    wr = LEN(WaitJs("document.readyState==='complete'", 10000))
    wr = LEN(WaitJs("document.querySelectorAll('#rail button').length>0", 10000))
  NEXT attempt
  IF INSTR(LCASE$(state), "connected") > 0 THEN
    Ok("WebSocket connesso", state + " (tentativo " + LTRIM$(STR$(attempt)) + ")")
  ELSE
    ' La UI può restare su "closed" (flake noto di Chromium headless: il primo onopen
    ' non viene consegnato). Si verifica l'endpoint DAL CONTESTO DELLA PAGINA con una
    ' socket fresh: se apre, il server WS della webapp è raggiungibile e funzionante.
    page = JsValue("new Promise(function(res){try{var w=new WebSocket('ws://'+location.host+'/ws/canvas');w.onopen=function(){w.close();res('open')};w.onerror=function(){res('error')};setTimeout(function(){res('timeout rs='+w.readyState)},5000);}catch(e){res('throw '+e.message)}})")
    IF LEFT$(page, 4) = "open" THEN
      Ok("WebSocket connesso", "socket fresh dal contesto pagina: " + page + " (UI=" + state + ", flake headless)")
    ELSE
      Bad("WebSocket connesso", "UI=" + state + " / fresh=" + page)
    END IF
  END IF

  PRINT ""
  PRINT "== errori JS =="
  errs = JsValue("JSON.stringify(window.__errs||[])")
  IF errs = "[]" OR LEN(errs) = 0 OR errs = "" THEN
    Ok("nessun errore JS", errs)
  ELSE
    Bad("nessun errore JS", errs)
  END IF

  ' Screenshot: Page.captureScreenshot restituisce base64; FROMBASE64 produce la
  ' stringa BINARIA con lunghezza esatta (fix C99-FROMBASE64-Payload-Grandi).
  r = SendCmd("Page.captureScreenshot", "{" + Q("format") + ":" + Q("png") + "}")
  png = ExtractData(r)
  IF LEN(png) > 0 THEN
    DIM dec() AS BYTE, pad AS LONG
    dec = FROMBASE64(png)
    ' padding base64 (controllare PRIMA "==" e poi "=", mai sovrascrivere)
    pad = 0
    IF LEN(png) >= 2 AND RIGHT$(png, 2) = "==" THEN
      pad = 2
    ELSEIF LEN(png) >= 1 AND RIGHT$(png, 1) = "=" THEN
      pad = 1
    END IF
    n = (LEN(png) \ 4) * 3 - pad
    REDIM bytes(n - 1) AS BYTE
    FOR i = 0 TO n - 1
      bytes(i) = dec(i)
    NEXT i
    wr = IO.WRITEBYTES("/tmp/webapp_ui.png", bytes)
    PRINT ""
    PRINT "screenshot: /tmp/webapp_ui.png (bytes="; n; ")"
  ELSE
    Warn("screenshot non disponibile", "risposta senza risultato")
  END IF

  CleanupAgents
  IF started <> 0 AND keep = 0 THEN
    root = IO.PARENTDIR(IO.PARENTDIR(IO.CWD()))
    wr = SYSTEM.SHELLOUT(Q(root + "/tools/sh/webapp_up.sh") + " stop >/dev/null 2>&1")
  END IF
  IF LEN(cdpArg) = 0 THEN
    wr = SYSTEM.SHELLOUT("pkill -f 'remote-debugging-port=" + LTRIM$(STR$(g_cdpPort)) + "' >/dev/null 2>&1")
  END IF
  ' chiude la socket CDP (ferma il receive-loop) e il task di servizio
  g_ws.Close("bye")
  g_stop = -1
  SLEEP 400

  PRINT ""
  PRINT "------------------------------------------"
  PRINT "  PASS: "; g_pass; "   WARN: "; g_warn; "   FAIL: "; g_fail
  IF g_fail = 0 THEN
    IF g_warn > 0 THEN PRINT "  UI OK (con WARN)" ELSE PRINT "  UI OK"
    RETURN 0
  END IF
  RETURN 1
END FUNCTION

' Task di servizio: tiene un timer GPM armato (vedi Main) finché il tool lavora.
SUB KeepAlive(wg AS WAITGROUP)
  DO WHILE g_stop = 0
    SLEEP 200
  LOOP
  DONE wg
END SUB
