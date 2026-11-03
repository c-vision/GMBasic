' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: Events/OnKeyUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: ONKEY - COSTANTI KEY NON ANCORA COPERte DAGLI ESEMPI
' =================================================================================================
' Questo test copre le costanti stile PowerBASIC (sempre disponibili senza IMPORT) che non
' comparivano in alcun esempio .bas: %K_ENT, %K_ALT, %K_SHIFT.
'
' COPERTURA:
' - %K_ENT        : tasto Invio (13).
' - %K_ALT      : modificatore ALT (1).
' - %K_SHIFT    : modificatore SHIFT (2).
' - KEY.MAKE(t, mod) : combina tasto + modificatori (già coperto, usato come verifica).
' =================================================================================================
IMPORT ONKEY

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "ONKEY - Costanti scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    ' 1. Valori numerici delle costanti
    PRINT "-- Valori costanti --"
    PRINT "%K_ENT     = "; %K_ENT
    PRINT "%K_ALT   = "; %K_ALT
    PRINT "%K_SHIFT = "; %K_SHIFT
    PRINT

    ' 2. Combinazioni con KEY.MAKE
    PRINT "-- Combinazioni --"
    PRINT "KEY.MAKE(13, 1)     = combinazione ENTER+ALT"
    PRINT "KEY.MAKE(112, 2)    = combinazione F1+SHIFT"
    PRINT

    ' 2b. Cifre D0-D9
    PRINT "-- Cifre %K_0..%K_9 --"
    PRINT "%K_0 = "; %K_0; " (48)"
    PRINT "%K_5 = "; %K_5; " (53)"
    PRINT "%K_9 = "; %K_9; " (57)"
    PRINT

    ' 3. Registrazione di handler ON KEY con le costanti
    PRINT "-- ON KEY con costanti --"
    ON KEY (%K_ENT) SalutoInvio()
    ON KEY (KEY.MAKE(112, 1)) SalutoAltF1()
    ON KEY (KEY.MAKE(112, 2)) SalutoShiftF1()
    PRINT "Handler registrati. Premi i tasti (oppure continua)."
    PRINT

    PRINT "Test Completato."
END SUB

SUB SalutoInvio()
    PRINT "Invio premuto."
END SUB

SUB SalutoAltF1()
    PRINT "Alt+F1 premuto."
END SUB

SUB SalutoShiftF1()
    PRINT "Shift+F1 premuto."
END SUB
