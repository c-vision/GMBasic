' ============================================================================
' FILE: APPS/ServiceConsole/src/ServiceConsole.bas
'
' ServiceConsole (GMBasic) - primo servizio con GUI.
'   * servizio: daemon HTTP con /healthz, /status, /config (keep-alive cooperativo)
'   * GUI: finestra TUI di stato con refresh periodico (in futuro i widget TUI
'     saranno polimorfici verso la GUI nativa pixel - vedi Knowledge/).
'
' MODI (simboli compile-time, vedi APPS/README.md):
'   (default)                       status: banner + config + stato
'   -DSERVICECONSOLE_MODE_DAEMON    daemon: HTTP + keep-alive
'   -DSERVICECONSOLE_MODE_GUI       GUI: finestra TUI con refresh
'
' CONFIG: serviceconsole.cfg (key=value) nella cartella dell'app.
' RUN (dalla ROOT del monorepo):
'   tools/sh/harness.sh il shared/APPS/ServiceConsole/src/ServiceConsole.bas
'   (modo) aggiungere in testa al file: #DEF SERVICECONSOLE_MODE_DAEMON  (o _GUI)
'          i runner di tools/runners/ non passano -D
' ============================================================================

IMPORT IO
IMPORT STRINGS
IMPORT HTTP
IMPORT JSON
IMPORT WINDOWS
IMPORT TRAY
IMPORT GRAPHICS

' ---- FFI: loop/form dei controlli TUI (modello GMBasicIde: NON sono builtin BASIC) ----
' Il registry dei controlli, il frame e lo stato di chiusura vivono in `libgmb_gfx`
' (`gmb_tui_controls_reset`, `gmb_ui_frame`, `gmb_form_closed`, `gmb_form_request_close`);
' il linguaggio NON ha `FORM_RESET/FORM_FRAME/FORM_CLOSED/FORM_CLOSE`.
DECLARE SUB      gmb_tui_controls_reset LIB "gmb_gfx" ()
DECLARE SUB      gmb_ui_frame           LIB "gmb_gfx" (ms AS LONG)
DECLARE FUNCTION gmb_form_closed        LIB "gmb_gfx" () AS LONG
DECLARE SUB      gmb_form_request_close LIB "gmb_gfx" ()

DIM g_name AS STRING
DIM g_port AS INTEGER
DIM g_services AS STRING
DIM g_cfgPath AS STRING
DIM g_ticks AS INTEGER
DIM g_status AS STRING
DIM g_mode AS STRING
DIM g_maxsec AS INTEGER
DIM g_trayOn AS LONG
DIM g_trayIcon AS STRING
DIM g_stop AS LONG
DIM g_svcName(8) AS STRING
DIM g_svcPort(8) AS INTEGER
DIM g_svcUp(8) AS INTEGER
DIM g_svcCount AS INTEGER
DIM g_svcOk AS INTEGER
' --- GUI (stato/log/selezione) ---
DIM g_guiSel AS INTEGER
DIM g_upSig AS STRING
DIM g_rebuild AS INTEGER
DIM g_winOpen AS INTEGER
DIM g_guiDrawn AS INTEGER
DIM g_guiPrevUp(8) AS INTEGER
DIM g_guiPrevSel AS INTEGER
DIM g_log(6) AS STRING
DIM g_logN AS INTEGER
DIM g_logHead AS INTEGER

SUB Main
  g_name = "service-console"
  g_port = 8770
  g_services = ""
  g_cfgPath = ""
  g_ticks = 0
  g_status = "starting"
  g_mode = "status"
  g_maxsec = 3600
  g_trayOn = 1
  g_trayIcon = ""
  g_stop = 0
  g_svcCount = 0
  g_svcOk = 0
  SET SHELL ON
  LoadConfig()
  g_mode = ComputeMode()
  g_status = "running"
  PrintBanner()
  IF g_mode = "daemon" THEN
    RunDaemon()
  END IF
  IF g_mode = "gui" THEN
    RunGui()
  END IF
END SUB

' Modo: dal config (mode=...), override compile-time con #IFDEF.
' (Il -D non arriva a tutti i runner: stesso pattern di APPS/Masume.)
FUNCTION ComputeMode() AS STRING
  DIM m AS STRING
  m = g_mode
#IFDEF SERVICECONSOLE_MODE_DAEMON
  m = "daemon"
#ENDIF
#IFDEF SERVICECONSOLE_MODE_GUI
  m = "gui"
#ENDIF
  ComputeMode = m
END FUNCTION

SUB PrintBanner
  PRINT "=============================================="
  PRINT " ServiceConsole - " + g_name
  PRINT " porta=" + LTRIM(STR(g_port)) + "  servizi=" + g_services
  PRINT " modo=" + g_mode + "  stato=" + g_status
  PRINT "=============================================="
END SUB

SUB LoadConfig
  DIM candidate AS STRING
  DIM dir AS STRING
  DIM t AS INTEGER
  candidate = IO.CWD() + "/serviceconsole.cfg"
  IF NOT IO.FILEEXISTS(candidate) THEN
    candidate = ""
    dir = IO.CWD()
    FOR t = 1 TO 6
      IF IO.FILEEXISTS(dir + "/APPS/ServiceConsole/serviceconsole.cfg") THEN
        candidate = dir + "/APPS/ServiceConsole/serviceconsole.cfg"
        EXIT FOR
      END IF
      dir = IO.PARENTDIR(dir)
    NEXT
  END IF
  IF LEN(candidate) = 0 THEN
    CPRINT "[cfg] serviceconsole.cfg non trovato - uso i default"
    EXIT SUB
  END IF
  g_cfgPath = candidate
  DIM lines = IO.READLINES(candidate)
  DIM i AS INTEGER
  DIM ln AS STRING
  DIM p AS INTEGER
  DIM k AS STRING
  DIM v AS STRING
  FOR i = 0 TO UBOUND(lines)
    ln = TRIM(lines(i))
    IF LEN(ln) > 0 AND LEFT(ln, 1) <> "#" THEN
      p = INSTR(ln, "=")
      IF p > 1 THEN
        k = TRIM(LEFT(ln, p - 1))
        v = TRIM(MID(ln, p + 1, LEN(ln) - p))
        IF k = "name" THEN g_name = v
        IF k = "port" THEN g_port = VAL(v)
        IF k = "services" THEN g_services = v
        IF k = "mode" THEN g_mode = v
        IF k = "maxsec" THEN g_maxsec = VAL(v)
        IF k = "tray" THEN g_trayOn = VAL(v)
        IF k = "icon" THEN g_trayIcon = v
        IF k = "services" THEN g_services = v
      END IF
    END IF
  NEXT
  CPRINT "[cfg] " + g_cfgPath
  ParseServices()
END SUB

' --- CONSOLE: elenco servizi da monitorare -----------------------------------
SUB ParseServices
  g_svcCount = 0
  DIM rest AS STRING
  rest = g_services
  WHILE LEN(rest) > 0
    DIM comma AS INTEGER
    comma = INSTR(rest, ",")
    DIM item AS STRING
    IF comma > 0 THEN
      item = LEFT(rest, comma - 1)
      rest = MID(rest, comma + 1, LEN(rest) - comma)
    ELSE
      item = rest
      rest = ""
    END IF
    item = TRIM(item)
    IF LEN(item) > 0 AND g_svcCount < 8 THEN
      DIM colon AS INTEGER
      colon = INSTR(item, ":")
      IF colon > 1 THEN
        g_svcName(g_svcCount) = TRIM(LEFT(item, colon - 1))
        g_svcPort(g_svcCount) = VAL(TRIM(MID(item, colon + 1, LEN(item) - colon)))
        g_svcUp(g_svcCount) = 0
        g_svcCount = g_svcCount + 1
      END IF
    END IF
  WEND
  IF g_svcCount > 0 THEN
    CPRINT "[console] servizi da monitorare: " + g_services
  END IF
END SUB

' Poll dei servizi: GET /healthz su ciascuno (HTTP.GET su porta chiusa -> "" = down).
SUB PollServices
  g_svcOk = 0
  DIM i AS INTEGER
  FOR i = 0 TO g_svcCount - 1
    DIM body AS STRING
    body = HTTP.GET("http://localhost:" + LTRIM(STR(g_svcPort(i))) + "/healthz")
    IF LEN(body) > 0 AND INSTR(body, "running") > 0 THEN
      g_svcUp(i) = 1
      g_svcOk = g_svcOk + 1
    ELSE
      g_svcUp(i) = 0
    END IF
  NEXT
END SUB

' --- SERVIZIO: daemon HTTP + keep-alive cooperativo --------------------------
SUB RunDaemon
  DIM cfg AS HTTPCONFIG
  cfg = NEW HTTPCONFIG
  cfg.Port = g_port
  HTTP CONFIG cfg
  HTTP ROUTE "GET", "/healthz", HealthHandler()
  HTTP ROUTE "GET", "/status", StatusHandler()
  HTTP ROUTE "GET", "/", DashboardHandler()
  HTTP ROUTE "GET", "/config", ConfigHandler()
  HTTP ROUTE "GET", "/status.txt", StatusTxtHandler()
  HTTP ROUTE "GET", "/control/stop", StopHandler()
  HTTP ROUTE "GET", "/control/start", StartHandler()
  HTTP ROUTE "GET", "/control/restart", RestartHandler()
  CPRINT "[daemon] in ascolto su http://localhost:" + LTRIM(STR(g_port))
  CPRINT "[daemon] endpoint: /healthz  /status  /config"
  HTTP SERVER cfg.Port
  TraySetup()
  ' keep-alive: 1h demo. Il lifecycle reale (start/stop/restart su SIGTERM)
  ' sara' del daemon manager (launchd/systemd) - vedi Knowledge/.
  DIM i AS INTEGER
  DIM cmd AS LONG
  FOR i = 1 TO g_maxsec
    SLEEP 1000
    g_ticks = g_ticks + 1
    TrayPump()
    cmd = TRAYLAST()
    TrayHandle(cmd)
    IF i - INT(i / 5) * 5 = 0 THEN
      PollServices()
      TrayTooltipUpdate()
    END IF
    IF g_stop = 1 THEN
      EXIT FOR
    END IF
  NEXT
  g_status = "stopping"
  TrayTeardown()
  HTTP SERVER STOP
  CPRINT "[daemon] fermato (ticks=" + STR(g_ticks) + ")"
END SUB

SUB HealthHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM h AS JSON
  h = NEW JSON
  h("status") = g_status
  h("uptime_s") = g_ticks
  res.SendJSON(h)
END SUB

SUB StatusHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM s AS JSON
  s = NEW JSON
  s("name") = g_name
  s("port") = g_port
  s("services") = g_services
  s("services_total") = g_svcCount
  s("services_up") = g_svcOk
  s("ticks") = g_ticks
  DIM det AS STRING
  det = ""
  DIM i AS INTEGER
  FOR i = 0 TO g_svcCount - 1
    IF g_svcUp(i) = 1 THEN
      det = det + g_svcName(i) + ":up"
    ELSE
      det = det + g_svcName(i) + ":down"
    END IF
    IF i < g_svcCount - 1 THEN
      det = det + ","
    END IF
  NEXT
  s("services_detail") = det
  res.SendJSON(s)
END SUB

' Testo semplice per la console TUI: righe "nome,porta,up|down".
SUB StatusTxtHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM t AS STRING
  t = "*" + g_name + "," + LTRIM(STR(g_port)) + ",up" + CHR(10)   ' "*" = il console stesso (non fermarlo)
  DIM i AS INTEGER
  FOR i = 0 TO g_svcCount - 1
    IF g_svcUp(i) = 1 THEN
      t = t + g_svcName(i) + "," + LTRIM(STR(g_svcPort(i))) + ",up" + CHR(10)
    ELSE
      t = t + g_svcName(i) + "," + LTRIM(STR(g_svcPort(i))) + ",down" + CHR(10)
    END IF
  NEXT
  res.ContentType = "text/plain"
  res.Send(t)
END SUB

' Controllo: avvia i servizi (daemon start). ?svc=<nome> = solo quello, altrimenti tutti.
SUB StartHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM svc AS STRING
  svc = req.Query("svc")
  IF LEN(TRIM(svc)) > 0 THEN
    StartOne(TRIM(svc))
    res.Send("starting " + TRIM(svc))
  ELSE
    StartServices()
    res.Send("starting")
  END IF
END SUB

' Controllo: riavvia (?svc=<nome> = solo quello, altrimenti tutti).
SUB RestartHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM svc AS STRING
  svc = TRIM(req.Query("svc"))
  DIM i AS INTEGER
  FOR i = 0 TO g_svcCount - 1
    IF LEN(svc) = 0 OR g_svcName(i) = svc THEN
      DIM b AS STRING
      b = HTTP.GET("http://localhost:" + LTRIM(STR(g_svcPort(i))) + "/control/stop")
      SLEEP 500
      StartOne(g_svcName(i))
    END IF
  NEXT
  res.Send("restarting")
END SUB

' Avvia un singolo servizio via daemon manager.
SUB StartOne(nm AS STRING)
  DIM si2 AS INTEGER
  FOR si2 = 0 TO g_svcCount - 1
    IF g_svcName(si2) = nm THEN
      g_svcUp(si2) = 1
    END IF
  NEXT si2
  DIM replDll AS STRING
  replDll = FindReplDll()
  IF LEN(replDll) = 0 THEN
    EXIT SUB
  END IF
  DIM ok AS LONG
  ok = ShellRun("dotnet " + replDll + " daemon start " + nm)
  IF ok = 1 THEN
  ELSE
  END IF
END SUB

' Controllo: ferma questo servizio (usato dal console/tray).
SUB StopHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  g_stop = 1
  res.Send("stopping")
END SUB

' Console 100%: nessun HTML. GET / = stato in testo semplice (come /status.txt + uptime).
SUB DashboardHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM t AS STRING
  t = g_name + " | porta " + LTRIM(STR(g_port)) + " | uptime " + LTRIM(STR(g_ticks)) + "s | " + LTRIM(STR(g_svcOk)) + "/" + LTRIM(STR(g_svcCount)) + " attivi" + CHR(10)
  DIM i AS INTEGER
  FOR i = 0 TO g_svcCount - 1
    DIM st AS STRING
    st = "down"
    IF g_svcUp(i) = 1 THEN
      st = "up"
    END IF
    t = t + g_svcName(i) + "," + LTRIM(STR(g_svcPort(i))) + "," + st + CHR(10)
  NEXT
  res.ContentType = "text/plain"
  res.Send(t)
END SUB

SUB ConfigHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM c AS JSON
  c = NEW JSON
  c("cfg_path") = g_cfgPath
  c("services") = g_services
  res.SendJSON(c)
END SUB

' --- GUI: console di controllo in-process (TUI) ------------------------------
' Tabella servizi + selezione + azioni + log. Unica logica di stato: PollServices
' (in-process, niente round-trip HTTP per lo stato). Tasti:
'   a avvia tutti · f ferma tutti · s stop sel · t start sel · r restart sel
'   1..8 selezione · q/ESC esci
' Pannello log (6 righe fisse, riga 16): scroll circolare.
SUB GuiLog(msg AS STRING)
  DIM i AS INTEGER
  IF g_logN < 6 THEN
    g_log(g_logN) = msg
    g_logN = g_logN + 1
  ELSE
    FOR i = 0 TO 4
      g_log(i) = g_log(i + 1)
    NEXT
    g_log(5) = msg
  END IF
  FOR i = 0 TO 5
    LOCATE 16 + i, 1
    DIM ln AS STRING
    ln = ""
    IF i < g_logN THEN
      ln = g_log(i)
    END IF
    PRINT LEFT(PADR(ln, 78), 78)
  NEXT
END SUB

' Frame statico: intestazione, colonne, area log, riga tasti.
SUB GuiDrawStatic
  WINDOW Console CLEAR
  DIM tr AS STRING
  tr = "off"
  IF g_trayOn = 1 THEN
    tr = "on"
  END IF
  WINDOW Console PRINT "ServiceConsole - " + g_name + "   [tray " + tr + "]"
  WINDOW Console PRINT "porta: " + LTRIM(STR(g_port)) + "   servizi: " + LTRIM(STR(g_svcCount)) + "   attivi: " + LTRIM(STR(g_svcOk)) + "   stato: " + g_status
  WINDOW Console PRINT ""
  WINDOW Console PRINT "  #  " + LEFT(PADR("nome", 26), 26) + " " + LEFT(PADR("porta", 7), 7) + " " + LEFT(PADR("stato", 6), 6)
  WINDOW Console PRINT "  -  " + LEFT(PADR("--------------------------", 26), 26) + " " + LEFT(PADR("-------", 7), 7) + " " + LEFT(PADR("------", 6), 6)
  DIM i AS INTEGER
  FOR i = 1 TO 11
    WINDOW Console PRINT ""
  NEXT
  WINDOW Console PRINT "log:"
  FOR i = 1 TO 6
    WINDOW Console PRINT ""
  NEXT
  WINDOW Console PRINT "[a] avvia tutti  [f] ferma tutti  [s] stop sel  [t] start sel  [r] restart sel  [1-8] sel  [q/ESC] esci"
END SUB

' Righe servizi (riga 6+idx): marcatore selezione + stato corrente.
SUB GuiDrawRows
  DIM i AS INTEGER
  FOR i = 0 TO 7
    LOCATE 6 + i, 1
    IF i < g_svcCount THEN
      DIM mk AS STRING
      mk = "  "
      IF i + 1 = g_guiSel THEN
        mk = "> "
      END IF
      DIM st AS STRING
      st = "-"
      IF g_svcUp(i) = 1 THEN
        st = "OK"
      END IF
      PRINT mk + LTRIM(STR(i + 1)) + "  " + LEFT(PADR(g_svcName(i), 26), 26) + " " + LEFT(PADR(LTRIM(STR(g_svcPort(i))), 7), 7) + " " + LEFT(PADR(st, 6), 6)
      g_guiPrevUp(i) = g_svcUp(i)
    ELSE
      PRINT LEFT(PADR("", 78), 78)
    END IF
  NEXT
  g_guiPrevSel = g_guiSel
END SUB

' Refresh: poll + aggiornamento IN-PLACE solo delle celle cambiate (LOCATE window-relative).
SUB GuiRefresh
  PollServices()
  DIM i AS INTEGER
  FOR i = 0 TO g_svcCount - 1
    DIM st AS STRING
    st = "-"
    IF g_svcUp(i) = 1 THEN
      st = "OK"
    END IF
    IF g_svcUp(i) <> g_guiPrevUp(i) THEN
      LOCATE 6 + i, 41
      PRINT LEFT(PADR(st, 6), 6)
      g_guiPrevUp(i) = g_svcUp(i)
      GuiLog("[" + LTRIM(STR(g_ticks)) + "] " + g_svcName(i) + " -> " + st)
    END IF
  NEXT
  LOCATE 2, 1
  PRINT LEFT(PADR("porta: " + LTRIM(STR(g_port)) + "   servizi: " + LTRIM(STR(g_svcCount)) + "   attivi: " + LTRIM(STR(g_svcOk)) + "   stato: " + g_status, 78), 78)
END SUB

' Stop di un singolo servizio via endpoint /control/stop.
SUB StopOne(idx AS INTEGER)
  DIM b AS STRING
  b = HTTP.GET("http://localhost:" + LTRIM(STR(g_svcPort(idx))) + "/control/stop")
  g_svcUp(idx) = 0
  GuiLog("[" + LTRIM(STR(g_ticks)) + "] stop " + g_svcName(idx) + " -> " + LEFT(b, 12))
  SLEEP 500
  PollServices()
END SUB

' Restart di un singolo servizio: stop + start (daemon manager).
SUB RestartOne(idx AS INTEGER)
  StopOne(idx)
  StartOne(g_svcName(idx))
  GuiLog("[" + LTRIM(STR(g_ticks)) + "] restart " + g_svcName(idx))
  SLEEP 1000
  PollServices()
END SUB

SUB RunGui
  ' All'avvio: SOLO icona nella menu bar. La finestra grafica si apre dal click su
  ' "Apri console" (id 2) e si richiude con ESC/Esci (si torna al tray).
  g_status = "gui"
  TraySetup()
  ' Avvio: SOLO icona in menu bar. La finestra grafica si apre da "Apri console" (id 2).
  DIM cmd AS LONG
  DO WHILE g_stop = 0
    SLEEP 100
    g_ticks = g_ticks + 1
    TrayPump()
    cmd = TRAYLAST()
    TrayHandle(cmd)
  LOOP
  TrayTeardown()
  g_status = "stopped"
END SUB

' Costruisce (o RICOSTRUISCE) i controlli del form: usata per aggiornare i colori
' della lista (UP verde / DOWN rosso) quando cambia lo stato dei servizi.
SUB GuiBuildForm()
  ' Il registry dei controlli si azzera via FFI (`gmb_tui_controls_reset`, vedi DECLARE in testa):
  ' NON è un builtin BASIC. Ricostruire la lista a ogni refresh è il pattern di GMBasicIde.
  DIM svcList AS STRING
  svcList = ""
  DIM si AS INTEGER
  FOR si = 0 TO g_svcCount - 1
    DIM mk AS STRING
    mk = "@r "
    IF g_svcUp(si) = 1 THEN
      mk = "@g "
    END IF
    IF si > 0 THEN
      svcList = svcList + "|"
    END IF
    svcList = svcList + mk + g_svcName(si) + "   :" + LTRIM(STR(g_svcPort(si)))
  NEXT si
  STATUSBAR 12, 10, 736, "ServiceConsole   -   " + LTRIM(STR(g_svcOk)) + "/" + LTRIM(STR(g_svcCount)) + " servizi attivi"
  LISTBOX 12, 40, 404, 344, svcList INTO g_guiSel
  STATUSBAR 12, 406, 736, "Seleziona un servizio nella lista   |   ESC o 'Esci' per chiudere"
  BUTTON 428, 40, 160, 34, "Avvia tutti" ON CLICK GuiStartAll STYLE "rounded radius=7 gradient=#16a34a,#065f46 fg=white"
  BUTTON 428, 82, 160, 34, "Ferma tutti" ON CLICK GuiStopAll STYLE "rounded radius=7 gradient=#dc2626,#7f1d1d fg=white"
  BUTTON 428, 124, 160, 34, "Avvia selezionato" ON CLICK GuiStartSel STYLE "rounded radius=7 bg=#1e3a8a fg=#e0f2fe border=1,#22d3ee"
  BUTTON 428, 166, 160, 34, "Ferma selezionato" ON CLICK GuiStopSel STYLE "rounded radius=7 bg=#334155 fg=#cbd5e1"
  BUTTON 428, 208, 160, 34, "Riavvia selezionato" ON CLICK GuiRestartSel STYLE "rounded radius=7 bg=#6366f1 fg=white"
  BUTTON 428, 250, 160, 34, "Aggiorna stato" ON CLICK GuiRefreshSt STYLE "rounded radius=7 bg=#0e7490 fg=white"
  BUTTON 428, 300, 160, 34, "Esci" ON CLICK GuiExit STYLE "rounded radius=7 bg=#0b1220 fg=#cbd5e1 border=1,#94a3b8"
END SUB

' Apre la FINESTRA grafica (SCREEN + FORM) e gira il suo loop finche' non
' viene chiusa (ESC/Esci); poi torna al tray. Chiamata dal tray (cmd=2).
SUB OpenConsoleWindow()
  g_winOpen = 1
  SCREEN 12, 760, 440, 32
  WINDOW SHOW
  WINDOW THEME "light"
  PALETTE 9, 158, 199, 235
  COLOR 0, 9
  CLS
  g_guiSel = 0
  PollServices()
  GuiBuildForm()
  DIM tGui AS LONG
  tGui = 0
  DO WHILE gmb_form_closed() = 0 AND g_stop = 0
    gmb_ui_frame(16)
    tGui = tGui + 1
    IF g_rebuild = 1 THEN
      g_rebuild = 0
      IF g_guiSel < 0 THEN g_guiSel = 0
      IF g_guiSel > g_svcCount - 1 THEN g_guiSel = g_svcCount - 1
      IF g_guiSel < 0 THEN g_guiSel = 0
      GuiBuildForm()
    END IF
    TrayPump()
    DIM cmd2 AS LONG
    cmd2 = TRAYLAST()
    IF cmd2 <> 2 THEN
      TrayHandle(cmd2)
    END IF
  LOOP
  ' Nasconde la finestra (NON la distrugge): il run loop Cocoa/tray resta vivo.
  WINDOW HIDE
  g_winOpen = 0
END SUB

SUB GuiStartAll()
  DIM i3 AS INTEGER
  FOR i3 = 0 TO g_svcCount - 1
    StartOne(g_svcName(i3))
  NEXT i3
  g_rebuild = 1
  ALERT("Avvio di tutti i servizi richiesto.")
END SUB
SUB GuiStopAll()
  DIM c AS LONG
  c = CONFIRM("Fermare TUTTI i servizi?")
  IF c = 1 THEN
    StopServices()
    g_rebuild = 1
    ALERT("Stop di tutti i servizi richiesto.")
  END IF
END SUB
SUB GuiStartSel()
  IF g_guiSel >= 0 AND g_guiSel <= g_svcCount - 1 THEN
    StartOne(g_svcName(g_guiSel))
    g_rebuild = 1
    ALERT("Avvio richiesto per: " + g_svcName(g_guiSel))
  END IF
END SUB
SUB GuiStopSel()
  IF g_guiSel >= 0 AND g_guiSel <= g_svcCount - 1 THEN
    StopOne(g_guiSel)
    g_rebuild = 1
    ALERT("Stop richiesto per: " + g_svcName(g_guiSel))
  END IF
END SUB
SUB GuiRestartSel()
  IF g_guiSel >= 0 AND g_guiSel <= g_svcCount - 1 THEN
    RestartOne(g_guiSel)
    g_rebuild = 1
    ALERT("Riavvio richiesto per: " + g_svcName(g_guiSel))
  END IF
END SUB
SUB GuiRefreshSt()
  PollServices()
  g_rebuild = 1
END SUB
SUB GuiExit()
  DIM c AS LONG
  c = CONFIRM("Chiudere la ServiceConsole?")
  IF c = 1 THEN
    gmb_form_request_close()
  END IF
END SUB

' --- TRAY: setup/pump/azioni/teardown (usati da daemon e GUI) ----------------
SUB TraySetup
  g_trayOn = g_trayOn
  IF g_trayOn = 0 THEN
    EXIT SUB
  END IF
  IF TRAYAVAILABLE() = 0 THEN
    EXIT SUB
  END IF
  DIM tr AS LONG
  tr = TRAYCREATE(g_trayIcon, g_name)
  tr = TRAYITEM(1, "Stato")
  tr = TRAYITEM(2, "Apri console")
  tr = TRAYSEP()
  tr = TRAYITEM(3, "Avvia servizi")
  tr = TRAYITEM(4, "Ferma servizi")
  tr = TRAYSEP()
  tr = TRAYITEM(99, "Esci")
  PollServices()
  TrayTooltipUpdate()
END SUB

SUB TrayPump
  DIM tp AS LONG
  tp = TRAYPOLL()
END SUB

SUB TrayTooltipUpdate
  IF g_trayOn = 0 THEN
    EXIT SUB
  END IF
  DIM tt AS LONG
  tt = TRAYTOOLTIP(g_name + " - " + STR(g_svcOk) + "/" + STR(g_svcCount) + " servizi attivi")
END SUB

SUB TrayHandle(cmd AS LONG)
  IF cmd = 1 THEN
    TrayTooltipUpdate()
  END IF
  IF cmd = 2 THEN
    IF g_winOpen = 0 THEN
      OpenConsoleWindow()
    END IF
  END IF
  IF cmd = 3 THEN
    StartServices()
  END IF
  IF cmd = 4 THEN
    StopServices()
  END IF
  IF cmd = 99 THEN
    g_stop = 1
  END IF
END SUB

' Avvia i servizi via daemon manager (launchd): richiede che siano installati
' con `gmb daemon install <nome> <file.bas>` (stesso nome del config).
SUB StartServices
  DIM replDll AS STRING
  replDll = FindReplDll()
  IF LEN(replDll) = 0 THEN
    EXIT SUB
  END IF
  DIM i AS INTEGER
  FOR i = 0 TO g_svcCount - 1
    StartOne(g_svcName(i))
  NEXT
  SLEEP 2000
  PollServices()
  TrayTooltipUpdate()
END SUB

' Ferma i servizi via endpoint di controllo (ogni servizio si ferma da solo).
SUB StopServices
  DIM i AS INTEGER
  FOR i = 0 TO g_svcCount - 1
    DIM b AS STRING
    b = HTTP.GET("http://localhost:" + LTRIM(STR(g_svcPort(i))) + "/control/stop")
    g_svcUp(i) = 0
  NEXT
  SLEEP 1000
  PollServices()
  TrayTooltipUpdate()
END SUB

' Esegue un comando shell in modo sicuro: 1 = ok, 0 = shell non disponibile.
FUNCTION ShellRun(cmd AS STRING) AS LONG
  ON ERROR GOTO EH
  DIM o AS STRING
  o = SYSTEM.SHELLOUT(cmd)
  ShellRun = 1
  GOTO FINE
EH:
  ON ERROR GOTO 0
  ShellRun = 0
FINE:
END FUNCTION

' Radice del repo (per script/console_tui.sh), risalendo dalla CWD.
FUNCTION FindRepoRoot() AS STRING
  DIM dir AS STRING
  DIM t AS INTEGER
  dir = IO.CWD()
  FOR t = 1 TO 8
    IF IO.FILEEXISTS(dir + "/APPS/ServiceConsole/console_tui.sh") THEN
      FindRepoRoot = dir
      EXIT FUNCTION
    END IF
    dir = IO.PARENTDIR(dir)
  NEXT
  FindRepoRoot = ""
END FUNCTION

' Trova REPL.dll risalendo dalla CWD (per `daemon start`).
FUNCTION FindReplDll() AS STRING
  DIM dir AS STRING
  DIM t AS INTEGER
  dir = IO.CWD()
  FOR t = 1 TO 8
    DIM cand AS STRING
    cand = dir + "/REPL/bin/Debug/net10.0/REPL.dll"
    IF IO.FILEEXISTS(cand) THEN
      FindReplDll = cand
      EXIT FUNCTION
    END IF
    dir = IO.PARENTDIR(dir)
  NEXT
  FindReplDll = ""
END FUNCTION

SUB TrayTeardown
  IF g_trayOn = 0 THEN
    EXIT SUB
  END IF
  TRAYREMOVE()
  TrayPump()
END SUB
