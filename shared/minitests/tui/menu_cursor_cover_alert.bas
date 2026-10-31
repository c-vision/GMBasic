' TAG: interactive
' MiniTest TUI — SOLO il MENU (barra + finestre dropdown).
' Scenario richiesto:
'   * costruisce un MENU (barra in alto);
'   * sotto c'e' del TESTO con CURSORE LAMPEGGIANTE (CodeArea);
'   * il cursore e' COPerto quando si apre una finestra (dropdown) del menu';
'   * ogni item attivato mostra un ALERT BOX col nome dell'item cliccato.
' Interattivo (finestra SDL a celle): si chiude con ESC / chiusura finestra.
'
' NOTA: le factory FFI `gmb_tui_add_*` restituiscono il PUNTATORE del controllo,
' non l'indice. Per riferirsi a un controllo si usa l'INDICE d'ordine di
' registrazione (qui: 0 = CodeArea, 1 = Menu).
' EXPECT: OK-menu-cursor-cover-alert
IMPORT CONTROLS

DECLARE FUNCTION gmb_tui_add_codearea   LIB "gmb_gfx" (x AS LONG, y AS LONG, w AS LONG, h AS LONG, csv AS STRING) AS LONG
DECLARE FUNCTION gmb_tui_add_menu       LIB "gmb_gfx" (x AS LONG, y AS LONG, w AS LONG, csv AS STRING) AS LONG
DECLARE SUB      gmb_tui_menu_set       LIB "gmb_gfx" (i AS LONG, spec AS STRING)
DECLARE FUNCTION gmb_tui_menu_poll      LIB "gmb_gfx" () AS LONG
DECLARE SUB      gmb_tui_controls_reset LIB "gmb_gfx" ()
DECLARE SUB      gmb_tui_control_focus  LIB "gmb_gfx" (i AS LONG)
DECLARE SUB      gmb_tui_controls_frame LIB "gmb_gfx" (ms AS LONG)
DECLARE SUB      gmb_tui_sdl_start      LIB "gmb_gfx" (w AS LONG, h AS LONG)
DECLARE FUNCTION gmb_tui_sdl_pump       LIB "gmb_gfx" () AS LONG
DECLARE SUB      gmb_tui_sdl_present    LIB "gmb_gfx" ()
DECLARE SUB      gmb_tui_sdl_stop       LIB "gmb_gfx" ()

DIM run AS LONG
DIM mm AS LONG
DIM mtop AS LONG
DIM msub AS LONG
DIM ign AS LONG
DIM ci AS LONG
DIM mi AS LONG

' Nome dell'item dato (top, sub) — rispecchia la spec passata a gmb_tui_menu_set.
SUB ItemName(top AS LONG, sub AS LONG)
    DIM nm AS STRING
    DIM r AS INTEGER
    nm = "(sconosciuto)"
    IF top = 0 AND sub = 0 THEN nm = "Apri"
    IF top = 0 AND sub = 1 THEN nm = "Salva"
    IF top = 0 AND sub = 2 THEN nm = "Esci"
    IF top = 1 AND sub = 0 THEN nm = "Copia"
    IF top = 1 AND sub = 1 THEN nm = "Taglia"
    IF top = 1 AND sub = 2 THEN nm = "Incolla"
    IF top = 2 AND sub = 0 THEN nm = "Trova"
    IF top = 2 AND sub = 1 THEN nm = "Sostituisci"
    ' ALERT BOX col nome dell'item cliccato.
    r = ALERT("Item cliccato: " + nm)
    PRINT "item="; nm
END SUB

SUB Main()
    DIM testo AS STRING

    gmb_tui_sdl_start(80, 24)            ' finestra SDL a CELLE
    gmb_tui_controls_reset()

    ' TESTO con cursore (CodeArea): il caret lampeggia nelle righe sotto il menu'.
    testo = "Riga 1: testo sotto la barra del menu'" + CHR(10)
    testo = testo + "Riga 2: qui lampeggia il cursore" + CHR(10)
    testo = testo + "Riga 3: apri un menu -> il cursore si spegne" + CHR(10)
    testo = testo + "Riga 4: chiudi il menu -> il cursore ritorna"

    ' Registra i controlli e usa gli INDICI (0=CodeArea, 1=Menu): le factory
    ' restituiscono il puntatore, non l'indice.
    ign = gmb_tui_add_codearea(0, 2, 78, 20, testo)
    ign = gmb_tui_add_menu(0, 0, 80, "")
    ci = 0
    mi = 1
    gmb_tui_menu_set(mi, "File:Apri|Salva|Esci;Edit:Copia|Taglia|Incolla;Search:Trova|Sostituisci")
    gmb_tui_control_focus(ci)

    run = 1
    DO WHILE run <> 0
        IF gmb_tui_sdl_pump() <> 0 THEN run = 0
        gmb_tui_controls_frame(16)
        gmb_tui_sdl_present()
        mm = gmb_tui_menu_poll()
        IF mm >= 0 AND mm < 1000000 THEN
            PRINT "POLL="; mm
            ' Argomenti in variabili (non espressioni inline): evita il bug IL
            ' "InvalidProgramException" su SUB con argomento-espressione.
            mtop = INT(mm / 100)
            msub = mm - mtop * 100
            ItemName(mtop, msub)
        END IF
    LOOP

    gmb_tui_sdl_stop()
    PRINT "OK-menu-cursor-cover-alert"
END SUB
