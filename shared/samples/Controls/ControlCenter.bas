' ============================================================================
' ControlCenter — sample COMPLETO delle superfici TUI/GUI implementate.
' ============================================================================
' Usa OGNI comando/funzione delle superfici coperte in questa sessione:
'   WINDOW  : THEME, DESKTOP, FRAME, SHADOW, SELECT, SET TITLE, DEFINE,
'             <nome> PRINT/TEXT/CLEAR/SCROLL/EDIT/DESKTOP/FRAME/SHADOW e azioni
'             ACTIVATE/DEACTIVATE/RELEASE/CLOSE/MINIMIZE/RESTORE
'   MOUSE   : ON/OFF, SHOW/HIDE, SET, LIMIT/UNLIMIT, CURSOR, WAIT
'   WIDGET  : STATUSBAR, CHECKBOX, TOGGLE, RADIO, SLIDER, NUMERICUPDOWN,
'             STARRATING, SCROLLBAR, SCROLLVIEW, LABEL, DATALIST, PROPERTYGRID, LOGVIEW, HEXVIEW, BIGTEXT, CODEAREA, REORDERLISTBOX, ALERTBOX, CONFIRMATIONBOX, FILEEXPLORER, DASHBOARD, TAB, LISTBOX, COMBOBOX, MENU (flat+nested), DATEPICKER,
'             COLORPICKER, GRID, TREEVIEW, TBROWSE, FILEDIALOG, SPINNER,
'             PRINTERDIALOG
'   STRINGS : PADL/PADR/PADC (2-arg fill=spazio e 3-arg)
'   HTTP    : CONFIG, ROUTE, SERVER/STOP, HTTP.GET, req.Query, res.ContentType,
'             res.Send, res.SendJSON
'   TRAY    : AVAILABLE, CREATE, ITEM, SEP, TOOLTIP, POLL, LAST, BACKEND,
'             SELFTEST, REMOVE
'
' Headless: i widget sono MODALI; il sample inietta SENDKEY CHR(27) prima di
' ognuno (su C99Native/canary e PCode si chiudono; IL/VM richiedono il layer
' input in rework). Il server HTTP usa la porta 8786 e viene fermato subito.
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Controls/ControlCenter.bas
'   (oppure con GMB_TUI_SCRIPT=ESC×N per i modali sui backend C).
' ============================================================================
IMPORT CONTROLS
IMPORT WINDOWS
IMPORT MOUSE
IMPORT HTTP
IMPORT STRINGS
IMPORT TRAY

SUB Main()
  PRINT "=== ControlCenter ==="

  ' --- STRINGS: padding (2-arg fill=spazio, 3-arg esplicito) ---
  PRINT "PAD=[" + PADR("nome", 10) + "][" + PADL("42", 6, "0") + "][" + PADC("mid", 7) + "]"

  ' --- WINDOW: globali + DEFINE + contenuto + azioni ---
  WINDOW THEME 1
  WINDOW DESKTOP 0
  WINDOW FRAME DOUBLE
  WINDOW SHADOW ON
  WINDOW DEFINE Main FROM 1, 1 TO 20, 78 TITLE "ControlCenter"
  WINDOW SELECT Main
  WINDOW Main ACTIVATE
  WINDOW Main CLEAR
  WINDOW Main PRINT "ControlCenter demo"
  WINDOW Main TEXT "contenuto della finestra"
  WINDOW Main SCROLL 1
  WINDOW Main EDIT ON
  WINDOW Main EDIT OFF
  WINDOW SET TITLE "ControlCenter v1"
  WINDOW Main FRAME ROUNDED
  WINDOW Main SHADOW OFF
  WINDOW Main DESKTOP 1
  WINDOW Main MINIMIZE
  WINDOW Main RESTORE
  WINDOW Main DEACTIVATE
  WINDOW Main RELEASE

  ' --- MOUSE: configurazione + posizione + limiti + cursore ---
  MOUSE ON
  MOUSE SHOW
  MOUSE SET 10, 5
  MOUSE LIMIT 0, 0, 80, 24
  MOUSE CURSOR 1
  MOUSE UNLIMIT
  MOUSE HIDE
  MOUSE OFF
  MOUSE WAIT

  ' --- WIDGET: tutti i 18 controlli (SENDKEY per la chiusura headless) ---
  DIM n AS LONG
  DIM s AS STRING
  DIM ps AS STRING
  DIM cp AS LONG
  DIM dx AS LONG
  STATUSBAR "ControlCenter - demo completa"
  SENDKEY CHR(27)
  CHECKBOX 2, 2, "abilita" INTO n
  SENDKEY CHR(27)
  TOGGLE 3, 2, "modalita" INTO n
  SENDKEY CHR(27)
  RADIO 4, 2, ["A", "B", "C"] INTO s
  SENDKEY CHR(27)
  SLIDER 5, 2, 40, 0, 100, 5 INTO n
  SENDKEY CHR(27)
  NUMERICUPDOWN 5, 44, 12, 0, 100, 5 INTO n
  SENDKEY CHR(27)
  STARRATING 5, 58, 12, 5, 3 INTO n
  SENDKEY CHR(27)
  SCROLLBAR 5, 72, 20, 0, 100, 25 INTO n
  SENDKEY CHR(27)
  SCROLLVIEW 6, 44, 30, 4, ["r1", "r2", "r3"] INTO n
  SENDKEY CHR(27)
  LABEL 7, 44, 30, "etichetta"
  DATALIST 8, 44, 30, 5, ["a", "b", "c"] INTO n
  SENDKEY CHR(27)
  PROPERTYGRID 9, 44, 30, 5, ["Nome", "Demo", "Versione", "1.0"] INTO n
  SENDKEY CHR(27)
  LOGVIEW 10, 44, 30, 5, ["avvio", "ok"] INTO n
  SENDKEY CHR(27)
  HEXVIEW 11, 44, 30, 5, "dump" INTO n
  SENDKEY CHR(27)
  BIGTEXT 12, 44, "GMB"
  CODEAREA 13, 44, 30, 5, ["x = 1", "y = 2"] INTO n
  SENDKEY CHR(27)
  REORDERLISTBOX 14, 44, 30, 5, ["a", "b", "c"] INTO n
  SENDKEY CHR(27)
  ALERTBOX 15, 44, 30, 5, "avviso"
  SENDKEY CHR(27)
  CONFIRMATIONBOX 16, 44, 30, 5, "confermi?" INTO n
  SENDKEY CHR(27)
  FILEEXPLORER 17, 44, 30, 6, "." INTO n
  SENDKEY CHR(27)
  DASHBOARD 18, 44, 30, 6, ["CPU", "42", "100", "%"] INTO n
  SENDKEY CHR(27)
  TAB 6, 2, 50, 3, ["Generale", "Avanzate", "Help"] INTO n
  SENDKEY CHR(27)
  LISTBOX 7, 2, 30, 5, ["Uno", "Due", "Tre"] INTO s
  SENDKEY CHR(27)
  COMBOBOX 8, 2, 20, ["IT", "FR", "DE"] INTO s
  SENDKEY CHR(27)
  MENU 9, 2, ["File", "Edit", "Help"] INTO s
  SENDKEY CHR(27)
  MENU 10, 2, [["File", "Nuovo", "Apri"], ["Edit", "Taglia", "Copia"]] INTO s
  SENDKEY CHR(27)
  DATEPICKER 11, 2 INTO s
  SENDKEY CHR(27)
  COLORPICKER 12, 2, 16 INTO n
  SENDKEY CHR(27)
  GRID 13, 2, 60, 5, ["Nome", "Valore"], ["a", "1", "b", "2"] INTO n
  SENDKEY CHR(27)
  TREEVIEW 14, 2, 30, 10, ["radice", "figlio"] INTO s
  SENDKEY CHR(27)
  TBROWSE 15, 2, 70, 15, ["Nome", "Valore"], ["a", "1", "b", "2"] INTO s
  SENDKEY CHR(27)
  FILEDIALOG "*.bas", "Apri file" INTO s
  SENDKEY CHR(27)
  SPINNER 16, 2, 0
  SENDKEY CHR(27)
  PRINTERDIALOG INTO ps, cp, dx
  PRINT "widget ok (printer=" + ps + " copie=" + LTRIM(STR(cp)) + ")"

  ' --- HTTP: server + route + query + risposta JSON ---
  DIM cfg AS HTTPCONFIG
  cfg = NEW HTTPCONFIG
  cfg.Port = 8786
  HTTP CONFIG cfg
  HTTP ROUTE "GET", "/info", InfoHandler()
  HTTP SERVER 8786
  SLEEP 800
  DIM r AS STRING
  r = HTTP.GET("http://localhost:8786/info?what=status")
  PRINT "HTTP=" + r
  HTTP SERVER STOP

  ' --- TRAY: tutte le funzioni ---
  IF TRAYAVAILABLE() <> 0 THEN
    DIM tr AS LONG
    tr = TRAYCREATE("", "ControlCenter")
    tr = TRAYTOOLTIP("ControlCenter - demo")
    tr = TRAYITEM(1, "Stato")
    tr = TRAYSEP()
    tr = TRAYITEM(99, "Esci")
    ' Poll ripetuto: la creazione NSStatusItem e' processata sul main-thread (async su
    ' macOS) — senza il loop SELFTEST puo' vedere g_os_created=0 (divergenza fittizia).
    DIM tp AS LONG
    DIM ti AS INTEGER
    FOR ti = 1 TO 10
      SLEEP 50
      tp = TRAYPOLL()
    NEXT
    DIM tl AS LONG
    tl = TRAYLAST()
    DIM tb AS STRING
    tb = TRAYBACKEND()
    DIM ts AS LONG
    ts = TRAYSELFTEST()
    tr = TRAYREMOVE()
    PRINT "TRAY=" + tb + " self=" + LTRIM(STR(ts))
  ELSE
    PRINT "TRAY non disponibile su questo host"
  END IF

  ' --- WINDOW: chiusura ---
  WINDOW Main CLOSE
  PRINT "=== ControlCenter fine ==="
END SUB

SUB InfoHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM w AS STRING
  w = req.Query("what")
  DIM o AS JSON
  o = NEW JSON
  o("what") = w
  o("n") = 7
  res.ContentType = "application/json"
  res.SendJSON(o)
END SUB
