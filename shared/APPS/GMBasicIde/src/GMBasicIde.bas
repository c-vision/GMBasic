' TAG: interactive
' FILE: APPS/GMBasicIde/src/GMBasicIde.bas
' ============================================================================
' GMBasic IDE (TUI) — IDE completo in BASIC, standalone (C99Native: .bas -> C -> binario).
' La LOGICA (analisi) e' in C (gmb_ide); i controlli TUI sono in C (registry gmb_tui_*).
' Qui si pilota tutto via FFI (DECLARE ... LIB "gmb_gfx") e si ricostruisce la UI ad ogni
' cambio (pattern "RegisterUI", come RegisterIde della REPL C#). Nessun nuovo builtin.
'
' Layout:
'   riga 0 : Label "File:" + TextInput (percorso)
'   riga 1 : TabBar (tab aperti)
'   riga 2 : CodeArea (sinistra) | ListBox (outline, destra)
'   ultima : StatusBar
' Tasti: F1 aiuto  F2 apri  F3 salva  F4 outline  F5 completion  F6 lint
'        F7 goto-def  F8 refs  F9 make  F10 nuovo tab  ESC esci.
'
' Compila/esegui:  tools/gmb_c99native_run.sh APPS/GMBasicIde/src/GMBasicIde.bas
' ============================================================================
IMPORT CONTROLS

' ---- FFI: modello IDE in C (gmb_ide) ----
DECLARE FUNCTION gmb_idef_tabopen    LIB "gmb_gfx" (name AS STRING, text AS STRING) AS LONG
DECLARE FUNCTION gmb_idef_tabcount   LIB "gmb_gfx" () AS LONG
DECLARE FUNCTION gmb_idef_tabname    LIB "gmb_gfx" (i AS LONG) AS STRING
DECLARE FUNCTION gmb_idef_tabactivate LIB "gmb_gfx" (i AS LONG) AS LONG
DECLARE FUNCTION gmb_idef_tabclose   LIB "gmb_gfx" (i AS LONG) AS LONG
DECLARE SUB      gmb_idef_settext    LIB "gmb_gfx" (text AS STRING)
DECLARE FUNCTION gmb_idef_text       LIB "gmb_gfx" (i AS LONG) AS STRING
DECLARE FUNCTION gmb_idef_outline    LIB "gmb_gfx" () AS STRING
DECLARE FUNCTION gmb_idef_lint       LIB "gmb_gfx" () AS STRING
DECLARE FUNCTION gmb_idef_complete   LIB "gmb_gfx" (prefix AS STRING) AS STRING
DECLARE FUNCTION gmb_idef_snippet    LIB "gmb_gfx" (kind AS STRING) AS STRING
DECLARE FUNCTION gmb_idef_gotodef    LIB "gmb_gfx" (name AS STRING) AS LONG
DECLARE FUNCTION gmb_idef_refs       LIB "gmb_gfx" (name AS STRING) AS STRING
DECLARE FUNCTION gmb_idef_makebas    LIB "gmb_gfx" (path AS STRING) AS LONG

' ---- FFI: TUI (registry controlli + frame) ----
DECLARE SUB      gmb_tui_controls_reset LIB "gmb_gfx" ()
DECLARE SUB      gmb_ui_frame          LIB "gmb_gfx" (ms AS LONG)
DECLARE FUNCTION gmb_form_closed       LIB "gmb_gfx" () AS LONG
DECLARE FUNCTION gmb_tui_host_key_pop  LIB "gmb_gfx" () AS LONG
DECLARE FUNCTION gmb_tui_add_label     LIB "gmb_gfx" (x AS LONG, y AS LONG, w AS LONG, t AS STRING) AS LONG
DECLARE FUNCTION gmb_tui_add_menu      LIB "gmb_gfx" (x AS LONG, y AS LONG, w AS LONG, csv AS STRING) AS LONG
DECLARE FUNCTION gmb_tui_add_textinput LIB "gmb_gfx" (x AS LONG, y AS LONG, w AS LONG, t AS STRING) AS LONG
DECLARE FUNCTION gmb_tui_add_tab       LIB "gmb_gfx" (x AS LONG, y AS LONG, w AS LONG, csv AS STRING) AS LONG
DECLARE FUNCTION gmb_tui_add_codearea  LIB "gmb_gfx" (x AS LONG, y AS LONG, w AS LONG, h AS LONG, csv AS STRING) AS LONG
DECLARE FUNCTION gmb_tui_add_listbox   LIB "gmb_gfx" (x AS LONG, y AS LONG, w AS LONG, h AS LONG, csv AS STRING) AS LONG
DECLARE FUNCTION gmb_tui_add_statusbar LIB "gmb_gfx" (x AS LONG, y AS LONG, w AS LONG, t AS STRING) AS LONG
DECLARE FUNCTION gmb_tui_control_text_ptr  LIB "gmb_gfx" (i AS LONG) AS STRING
DECLARE SUB      gmb_tui_control_set_text  LIB "gmb_gfx" (i AS LONG, t AS STRING)
DECLARE SUB      gmb_tui_control_set_errors LIB "gmb_gfx" (i AS LONG, lines_1based AS STRING)
DECLARE FUNCTION gmb_code_area_goto_line   LIB "gmb_gfx" (i AS LONG, line AS LONG) AS LONG
DECLARE FUNCTION gmb_tui_control_value LIB "gmb_gfx" (i AS LONG) AS LONG
DECLARE FUNCTION gmb_tui_screen_width  LIB "gmb_gfx" () AS LONG
DECLARE FUNCTION gmb_tui_screen_height LIB "gmb_gfx" () AS LONG
DECLARE SUB      gmb_tui_show_cursor   LIB "gmb_gfx" ()
DECLARE SUB      gmb_raw_input_reader_shutdown LIB "gmb_gfx" ()
DECLARE SUB      gmb_tui_control_focus LIB "gmb_gfx" (i AS LONG)
DECLARE SUB      gmb_ui_set_theme_name LIB "gmb_gfx" (name AS STRING)
DECLARE SUB      gmb_tui_sdl_start   LIB "gmb_gfx" (w AS LONG, h AS LONG)
DECLARE FUNCTION gmb_tui_sdl_pump    LIB "gmb_gfx" () AS LONG
DECLARE SUB      gmb_tui_sdl_present LIB "gmb_gfx" ()
DECLARE SUB      gmb_tui_sdl_cursor_solid LIB "gmb_gfx" ()
DECLARE SUB      gmb_tui_sdl_stop    LIB "gmb_gfx" ()
DECLARE SUB      gmb_tui_controls_frame LIB "gmb_gfx" (ms AS LONG)
DECLARE FUNCTION gmb_tui_menu_poll LIB "gmb_gfx" () AS LONG
DECLARE SUB      gmb_tui_menu_set  LIB "gmb_gfx" (idx AS LONG, spec AS STRING)
DECLARE SUB      gmb_control_apply_style_handle LIB "gmb_gfx" (handle AS LONG, spec AS STRING)
DECLARE FUNCTION gmb_now_ms LIB "gmb_gfx" () AS LONG
DECLARE FUNCTION gmb_tui_host_mouse_pop  LIB "gmb_gfx" () AS LONG
DECLARE FUNCTION gmb_tui_host_mouse_x    LIB "gmb_gfx" () AS LONG
DECLARE FUNCTION gmb_tui_host_mouse_y    LIB "gmb_gfx" () AS LONG
DECLARE FUNCTION gmb_tui_host_mouse_ctrl LIB "gmb_gfx" () AS LONG

' Indici fissi dei controlli (ordine di creazione in RegisterUI).
DIM gW AS LONG : DIM gH AS LONG
DIM gPath AS STRING
DIM gCode AS STRING          ' buffer corrente (righe unite con CHR(10))
DIM gOutline AS STRING       ' voci outline unite con "|"
DIM gStatus AS STRING
DIM gHint AS STRING          ' menu' hot-key di default (ricompare dopo il time-out)
DIM gStatusFrames AS LONG    ' frame trascorsi col messaggio corrente (per il time-out)
DIM gTabs AS STRING          ' nomi tab uniti con "|"
DIM gRun AS BOOLEAN
DIM mk AS LONG
DIM jj AS LONG
DIM k AS LONG
DIM mm AS LONG
' Indici dei controlli (impostati in RegisterUI) — cosi' i riferimenti non dipendono da costanti.
DIM gMenuIdx AS LONG
DIM gTabIdx AS LONG
DIM gCodeIdx AS LONG
DIM gListIdx AS LONG
DIM gStatIdx AS LONG

' Converte la lista outline di gmb_ide ("line|kind|name;...") in una lista CSV "|" di NOMI.
FUNCTION OutlineNames(s AS STRING) AS STRING
    DIM res AS STRING
    DIM start AS LONG
    DIM semi AS LONG
    DIM item AS STRING
    DIM p1 AS LONG
    DIM p2 AS LONG
    DIM nm AS STRING
    start = 1
    DO WHILE start <= LEN(s)
        semi = INSTR(MID(s, start, LEN(s) - start + 1), ";")
        IF semi = 0 THEN
            item = MID(s, start, LEN(s) - start + 1)
            start = LEN(s) + 2
        ELSE
            item = MID(s, start, semi - 1)
            start = start + semi
        END IF
        p2 = 0
        p1 = INSTR(item, "|")
        DO WHILE p1 > 0
            p2 = p2 + p1
            p1 = INSTR(MID(item, p2 + 1, LEN(item) - p2 + 1), "|")
        LOOP
        IF p2 > 0 THEN nm = MID(item, p2 + 1, LEN(item) - p2 + 1) ELSE nm = item
        IF nm <> "" THEN
            IF res <> "" THEN res = res + "|"
            res = res + nm
        END IF
    LOOP
    OutlineNames = res
END FUNCTION

' Carica il buffer gCode nel modello C e aggiorna outline + squiggle di lint.
SUB SyncAnalysis()
    gmb_idef_settext(gCode)
    gOutline = OutlineNames(gmb_idef_outline())
END SUB

' Legge il testo del CodeArea (modifiche dell'utente) nel buffer gCode.
SUB PullCode()
    DIM t AS STRING
    t = gmb_tui_control_text_ptr(3)
    IF t <> "" THEN gCode = t
END SUB

' Ricostruisce la UI dal stato corrente (reset + creazione, come RegisterIde della REPL).
SUB RegisterUI()
    DIM li AS LONG
    gmb_tui_controls_reset()
    gTabIdx = 0
    gCodeIdx = 1
    gListIdx = 2
    gStatIdx = 3
    gMenuIdx = 4
    li = gmb_tui_add_tab(0, 1, gW, gTabs)
    li = gmb_tui_add_codearea(0, 2, gW * 70 / 100, gH - 3, "")
    gmb_tui_control_set_text(gCodeIdx, gCode)
    li = gmb_tui_add_listbox(gW * 70 / 100 + 1, 2, gW - gW * 70 / 100 - 1, gH - 3, gOutline)
    li = gmb_tui_add_statusbar(0, gH - 1, gW, gStatus)
    ' squiggle: righe 1-based degli errori di lint
    gmb_tui_control_set_errors(gCodeIdx, LintLines())
    ' Il MENU per ULTIMO: la sua dropdown va disegnata SOPRA gli altri controlli (z-order).
    li = gmb_tui_add_menu(0, 0, gW, "")
    gmb_tui_menu_set(gMenuIdx, "File:Apri|Salva|Esci;Edit:Copia|Taglia|Incolla;Search:Trova|Sostituisci;View:Outline|Lint|Refs;Build:Esegui|Compila")
    gmb_tui_control_focus(gCodeIdx)   ' focus sul CodeArea
END SUB

' Riga (1-based) degli errori di lint, unite con "|" (per gmb_tui_control_set_errors).
FUNCTION LintLines() AS STRING
    DIM s AS STRING
    DIM res AS STRING
    DIM start AS LONG
    DIM semi AS LONG
    DIM item AS STRING
    DIM bar AS LONG
    DIM ln AS LONG
    s = gmb_idef_lint()
    start = 1
    DO WHILE start <= LEN(s)
        semi = INSTR(MID(s, start, LEN(s) - start + 1), ";")
        IF semi = 0 THEN
            item = MID(s, start, LEN(s) - start + 1)
            start = LEN(s) + 2
        ELSE
            item = MID(s, start, semi - 1)
            start = start + semi
        END IF
        bar = INSTR(item, "|")
        IF bar > 1 THEN
            ln = VAL(MID(item, 1, bar - 1)) + 1
            IF res <> "" THEN res = res + "|"
            res = res + STR(ln)
        END IF
    LOOP
    LintLines = res
END FUNCTION

SUB SetStatus(m AS STRING)
    gStatus = m
    gStatusFrames = 0
    gmb_tui_control_set_text(gStatIdx, m)   ' statusbar (aggiornamento in place, senza ricostruire la UI)
END SUB

SUB ShowHint()
    SetStatus(gHint)
END SUB

' N-esimo elemento (1-based) di una lista separata da '|'.
FUNCTION NthItem(list AS STRING, n AS LONG) AS STRING
    DIM start AS LONG
    DIM pos AS LONG
    DIM i AS LONG
    DIM item AS STRING
    start = 1
    i = 1
    DO WHILE start <= LEN(list)
        pos = INSTR(MID(list, start, LEN(list) - start + 1), "|")
        IF pos = 0 THEN
            item = MID(list, start, LEN(list) - start + 1)
            start = LEN(list) + 2
        ELSE
            item = MID(list, start, pos - 1)
            start = start + pos
        END IF
        IF i = n THEN
            NthItem = item
            EXIT FUNCTION
        END IF
        i = i + 1
    LOOP
    NthItem = ""
END FUNCTION

' Azione di una voce di menu attivata (m = top*100 + sub).
SUB HandleMenu(m AS LONG)
    DIM top AS LONG
    DIM sub AS LONG
    DIM r AS LONG
    DIM mp AS STRING
    top = INT(m / 100)
    sub = m - top * 100
    IF top = 0 THEN                       ' File
        IF sub = 0 THEN
            gPath = gPath
            LoadFile()
        ELSEIF sub = 1 THEN
            gPath = gPath
            SaveFile()
            RegisterUI()
        ELSEIF sub = 2 THEN
            gRun = FALSE
        END IF
    ELSEIF top = 1 THEN                   ' Edit
        SetStatus("Edit: usa le scorciatoie sul testo")
    ELSEIF top = 2 THEN                   ' Search
        IF sub = 0 THEN
            SyncAnalysis()
            SetStatus("Trova: scrivi il testo nel campo in alto, poi F8 refs / F7 goto")
        ELSE
            SetStatus("Sostituisci: in arrivo")
        END IF
    ELSEIF top = 3 THEN                   ' View
        IF sub = 0 THEN
            SyncAnalysis()
            SetStatus("Outline: " + gOutline)
            RegisterUI()
        ELSEIF sub = 1 THEN
            SyncAnalysis()
            SetStatus("Lint: " + gmb_idef_lint())
            RegisterUI()
        ELSE
            SyncAnalysis()
            SetStatus("Refs(" + gPath + "): " + gmb_idef_refs(gPath))
        END IF
    ELSEIF top = 4 THEN                   ' Build
        SyncAnalysis()
        IF sub = 0 THEN
            SetStatus("Esegui: usa il comando Esegui della REPL")
        ELSE
            IF gPath <> "" THEN mp = gPath + ".build.bas" ELSE mp = "ideout.bas"
            r = gmb_idef_makebas(mp)
            SetStatus("Compila -> " + mp + " : " + STR(r))
        END IF
    END IF
END SUB

' Click mouse (celle; mc = indice del controllo sotto il cursore).
SUB HandleMouseClick(mx AS LONG, my AS LONG, mc AS LONG)
    DIM r AS LONG
    DIM nm AS STRING
    IF mc = gListIdx THEN                       ' ListBox outline: click su una voce -> goto
        r = my - 2
        nm = NthItem(gOutline, r + 1)
        IF nm <> "" THEN
            r = gmb_idef_gotodef(nm)
            IF r >= 0 AND r < 1000000 THEN
                r = gmb_code_area_goto_line(gCodeIdx, r)
                SetStatus("Goto: " + nm)
            END IF
        END IF
    ELSEIF mc = gTabIdx THEN                   ' TabBar: click -> attiva il tab
        r = gmb_tui_control_value(gTabIdx)
        IF r >= 0 AND r < 1000000 THEN
            gmb_idef_tabactivate(r)
            SetStatus("Tab " + STR(r + 1))
            RegisterUI()
        END IF
    ELSEIF mc = gCodeIdx THEN                   ' CodeArea: click -> sposta il cursore sulla riga
        r = gmb_code_area_goto_line(gCodeIdx, my - 2)
    END IF
END SUB

' I/O file (BASIC): legge tutte le righe del file in gCode.
' I/O file (BASIC), forma canonica `AS #1` + guardie (mai handle invalido).
SUB LoadFile()
    DIM line AS STRING
    DIM out AS STRING
    IF gPath = "" THEN
        SetStatus("Nessun file: scrivi il percorso nel campo File:")
        EXIT SUB
    END IF
    IF IO.FileExists(gPath) = 0 THEN
        SetStatus("File non trovato: " + gPath)
        EXIT SUB
    END IF
    OPEN gPath FOR INPUT AS #1
    DO WHILE NOT EOF(1)
        LINE INPUT #1, line
        IF out <> "" THEN out = out + CHR(10)
        out = out + line
    LOOP
    CLOSE #1
    gCode = out
    gmb_idef_tabopen(BaseName(gPath), gCode)
    SyncAnalysis()
    SetStatus("Aperto: " + gPath)
    RegisterUI()
END SUB

' Salva il buffer corrente nel percorso (letto dal CodeArea).
SUB SaveFile()
    DIM rest AS STRING
    DIM pos AS LONG
    IF gPath = "" THEN
        SetStatus("Nessun file: scrivi il percorso nel campo File:")
        EXIT SUB
    END IF
    PullCode()
    OPEN gPath FOR OUTPUT AS #1
    rest = gCode
    DO WHILE rest <> ""
        pos = INSTR(rest, CHR(10))
        IF pos = 0 THEN
            PRINT #1, rest
            rest = ""
        ELSE
            PRINT #1, MID(rest, 1, pos - 1)
            rest = MID(rest, pos + 1, LEN(rest) - pos)
        END IF
    LOOP
    CLOSE #1
    SetStatus("Salvato: " + gPath)
END SUB

FUNCTION BaseName(p AS STRING) AS STRING
    DIM i AS LONG
    DIM slash AS LONG
    i = 1
    slash = 0
    DO WHILE i <= LEN(p)
        IF MID(p, i, 1) = "/" THEN slash = i
        i = i + 1
    LOOP
    BaseName = MID(p, slash + 1, LEN(p) - slash)
END FUNCTION

' Riempie l'elenco dei tab (nomi "|"-uniti) dal modello C.
SUB RefreshTabs()
    DIM n AS LONG
    DIM i AS LONG
    DIM t AS STRING
    n = gmb_idef_tabcount()
    t = ""
    i = 0
    DO WHILE i < n
        IF t <> "" THEN t = t + "|"
        t = t + gmb_idef_tabname(i)
        i = i + 1
    LOOP
    gTabs = t
    IF gTabs = "" THEN gTabs = "(vuoto)"
END SUB

SUB HandleKey(k AS LONG)
    DIM r AS LONG
    DIM s AS STRING
    DIM mp AS STRING
    PullCode()
    IF k = 300 THEN
        SetStatus("GMBasic IDE | F2 apri F3 salva F4 outline F5 completion F6 lint F7 goto F8 refs F9 make F10 nuovo-tab ESC esci")
    ELSEIF k = 301 THEN
        gPath = gPath
        LoadFile()
    ELSEIF k = 302 THEN
        SaveFile()
        RegisterUI()
    ELSEIF k = 303 THEN
        SyncAnalysis()
        SetStatus("Outline: " + gOutline)
        RegisterUI()
    ELSEIF k = 304 THEN
        SyncAnalysis()
        SetStatus("Completion: " + gmb_idef_complete("PR"))
    ELSEIF k = 305 THEN
        SyncAnalysis()
        s = gmb_idef_lint()
        IF s = "" THEN s = "(nessun errore)"
        SetStatus("Lint: " + s)
        RegisterUI()
    ELSEIF k = 306 THEN
        SyncAnalysis()
        s = gmb_tui_control_text_ptr(4)
        IF s <> "" THEN
            r = gmb_idef_gotodef(s)
            IF r >= 0 AND r < 1000000 THEN
                r = gmb_code_area_goto_line(gCodeIdx, r)
                SetStatus("Goto: " + s + " -> riga " + STR(r + 1))
            ELSE
                SetStatus("Goto: simbolo non trovato: " + s)
            END IF
        ELSE
            SetStatus("Goto: seleziona un simbolo nell'outline.")
        END IF
    ELSEIF k = 307 THEN
        SyncAnalysis()
        s = gmb_tui_control_text_ptr(4)
        IF s = "" THEN s = "Main"
        SetStatus("Refs(" + s + "): " + gmb_idef_refs(s))
    ELSEIF k = 308 THEN
        SyncAnalysis()
        IF gPath <> "" THEN mp = gPath + ".build.bas" ELSE mp = "ideout.bas"
        r = gmb_idef_makebas(mp)
        SetStatus("Make -> " + mp + " : " + STR(r))
    ELSEIF k = 309 THEN
        gmb_idef_tabopen("untitled-" + STR(gmb_idef_tabcount()), "")
        RefreshTabs()
        SetStatus("Nuovo tab")
        RegisterUI()
    END IF
END SUB

SUB Main()
    gmb_ui_set_theme_name("turbo")   ' tema TurboVision
    gmb_tui_sdl_start(120, 40)       ' RICHIESTA finestra SDL a CELLE (creata dall'host, main thread)
    gW = 120 : gH = 40
    gPath = ""   ' nessun file di default: scrivilo nel campo File: (poi F2 apri / F3 salva)
    gCode = "SUB Main()" + CHR(10) + "    DIM x AS LONG" + CHR(10) + "    PRINT x" + CHR(10) + "END SUB"
    gHint = "GMBasic IDE | F2 apri F3 salva F4 outline F5 completion F6 lint F7 goto F8 refs F9 make F10 nuovo-tab ESC esci"
    gStatus = gHint
    gmb_idef_tabopen("untitled", gCode)
    SyncAnalysis()
    RefreshTabs()
    RegisterUI()

    gRun = TRUE
    DO WHILE gRun
        IF gmb_tui_sdl_pump() <> 0 THEN gRun = FALSE
        gmb_tui_controls_frame(16)
        gmb_tui_sdl_present()
        ' Il menu' degli hot-key ricompare dopo ~5s (time-out) o alla pressione di un tasto.
        gStatusFrames = gStatusFrames + 1
        IF gStatus <> gHint AND gStatusFrames > 300 THEN ShowHint()
        ' Voce di menu attivata (menu TV in alto): l'app esegue l'azione.
        mm = gmb_tui_menu_poll()
        IF mm >= 0 AND mm < 1000000 THEN HandleMenu(mm)
        ' Eventi mouse: l'app decide l'azione (click sull'outline -> goto, sul tab -> attiva).
        ' NB: la FFI puo' rendere un -1 come 4294967295 -> la sentinella "vuoto" va testata
        ' in positivo (kind validi = 1 down / 2 up), non con `< 0`.
        FOR jj = 1 TO 32
            mk = gmb_tui_host_mouse_pop()
            IF mk <> 1 AND mk <> 2 THEN EXIT FOR
            IF mk = 1 THEN HandleMouseClick(gmb_tui_host_mouse_x(), gmb_tui_host_mouse_y(), gmb_tui_host_mouse_ctrl())
        NEXT jj
        k = gmb_tui_host_key_pop()
        IF k = 27 OR gmb_form_closed() <> 0 THEN
            gRun = FALSE
        ELSEIF k <> 0 THEN
            gmb_tui_sdl_cursor_solid()   ' cursore solido mentre si digita
            ShowHint()
            HandleKey(k)
        END IF
    LOOP
    gmb_tui_sdl_stop()
END SUB
