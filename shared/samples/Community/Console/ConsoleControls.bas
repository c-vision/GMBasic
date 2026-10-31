' FILE: Console/ConsoleControls.bas
' =================================================================================================
' TEST UNITARIO: CONSOLE - COMANDI NON-BLOCCANTI (PORTABILI IN GMBASICAI)
' =================================================================================================
' Questo test copre i comandi CONSOLE che NON bloccano in attesa di input e sono implementati
' sia in GM-Basic che in GMBasicAI. I controlli UI interattivi (CHECKBOX/LISTBOX/MENU/...)
' stanno in Console/ConsoleUiControls.bas (escluso dal runner perché richiede input).
'
' NIENTE prefisso C* qui: CCLS/CLOCATE/CPRINT/CSAVESCREEN/CRESTORESCREEN/CPALETTE/
' FGRGB/BGRGB stanno in Graphics/GraphicsConsoleModes.bas (non portabili in GMBasicAI).
'
' COPERTURA:
' - ALERT "msg"                 : avviso modale (completa con default in headless).
' - v = CONFIRM("msg?")          : conferma -> %OK/%CANCEL/0.
' - CONSOLE.CURSOR("ON"/"OFF")  : visibilità cursore.
' - CONSOLE.CAPTION("titolo")   : titolo finestra console.
' - CONSOLE.SCROLLMODE(...)     : modalità scrollback/buffer.
' - CLIP.SET / CLIP.GET / CLIP CLEAR : appunti.
' =================================================================================================

IMPORT CLIP
SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "CONSOLE - Comandi non-bloccanti (portabili in GMBasicAI)"
    PRINT "============================================================"
    PRINT

    ' --- ALERT / CONFIRM ---
    PRINT "-- ALERT / CONFIRM --"
    DIM rr = ALERT("Test alert message!")
    DIM confirmVal AS BOOLEAN
    confirmVal = FALSE
    confirmVal = CONFIRM("Continuare?") = %OK
    PRINT "CONFIRM risultato: "; confirmVal
    PRINT

    ' --- CURSOR / CAPTION ---
    PRINT "-- CURSOR / CAPTION --"
    CURSOR ON
    CURSOR OFF
    CAPTION "GM-Basic Console Test"
    PRINT "CURSOR/CAPTION eseguiti."
    PRINT

    ' --- SCROLLMODE ---
    PRINT "-- SCROLLMODE --"
    SCROLLMODE "BUFFERED"
    PRINT "Modalita' BUFFERED attiva."
    SCROLLMODE "NATIVE"
    PRINT "Modalita' NATIVE ripristinata."
    PRINT

    ' --- CLIP ---
    PRINT "-- CLIP --"
    CLIP SET "testo negli appunti"
    DIM clipText$
    clipText$ = CLIP.GET()
    PRINT "CLIP.GET() = '"; clipText$; "'"
    CLIP CLEAR
    PRINT

    ' --- FGRGB / BGRGB (testo true-color ANSI) ---
    PRINT "-- FGRGB / BGRGB --"
    DIM fg$ = FGRGB("testo fg", 255, 255, 0)
    DIM bg$ = BGRGB("testo bg", 0, 0, 128)
    PRINT "FGRGB lunghezza: "; LEN(fg$); " (ANSI true-color foreground)"
    PRINT "BGRGB lunghezza: "; LEN(bg$); " (ANSI true-color background)"
    IF LEN(fg$) > 0 AND LEN(bg$) > 0 THEN PRINT "FGRGB/BGRGB: OK"
    PRINT

    PRINT "Test Completato."
END SUB
