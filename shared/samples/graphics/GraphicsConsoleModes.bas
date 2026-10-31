' FILE: Graphics/GraphicsConsoleModes.bas
' =================================================================================================
' TEST UNITARIO: COMANDI CONSOLE "C*" (SOLO GM-BASIC, NON PORTABILI IN GMBASICAI)
' =================================================================================================
' Questi comandi hanno il prefisso C* (Console) per distinguere l'operazione sulla console
' testuale da quella sulla finestra grafica. GMBasicAI NON ha la grafica, quindi questi
' comandi NON esistono lì (CLS/LOCATE/PRINT/SAVESCREEN/RESTORESCREEN senza prefisso sono
' la forma portabile). Stanno in Samples/Graphics perché restano solo in GM-Basic.
'
' COPERTURA (solo GM-Basic):
' - CCLS                 : pulisce SOLO la console testuale (CCLS).
' - CLOCATE r, c)        : posiziona il cursore SOLO console.
' - CPRINT "testo"       : stampa SOLO console.
' - CSAVESCREEN name)    : salva lo schermo console.
' - CRESTORESCREEN name) : ripristina lo schermo console.
' - CPALETTE i, r, g, b) : imposta la palette console.
' (FGRGB/BGRGB sono console true-color ANSI — testati in Console/ConsoleControls.bas.)
' =================================================================================================

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "CONSOLE C* - Modalita' console (solo GM-Basic)"
    PRINT "============================================================"
    PRINT

    ' --- CCLS (clear console-only) ---
    PRINT "-- CCLS --"
CCLS
    PRINT "CCLS: console pulita (solo console, non la finestra grafica)."
    PRINT

    ' --- CLOCATE (console-only cursor) ---
    PRINT "-- CLOCATE --"
CLOCATE 5, 1
    PRINT "CLOCATE: cursore a riga 5, colonna 1."
CLOCATE 7, 1
    PRINT

    ' --- CPRINT (console-only print) ---
    PRINT "-- CPRINT --"
CPRINT "Stampato solo sulla console (CPRINT)."
CPRINT ""
    PRINT

    ' --- CSAVESCREEN / CRESTORESCREEN (console-only) ---
    PRINT "-- CSAVESCREEN / CRESTORESCREEN --"
CSAVESCREEN "sc1"
CLOCATE 10, 1
    PRINT "Testo temporaneo dopo SAVE..."
CRESTORESCREEN "sc1"
    PRINT "CRESTORESCREEN: schermo console ripristinato."
    PRINT

    ' --- CPALETTE (console palette) ---
    PRINT "-- CPALETTE --"
CPALETTE 0, 255, 0, 0
    PRINT "CPALETTE: indice 0 -> rosso."
CPALETTE 1, 0, 255, 0
    PRINT "CPALETTE: indice 1 -> verde."
    PRINT

    PRINT "Test Completato."
END SUB
