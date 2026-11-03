' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: Console/ClipboardUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: CONSOLE - SUB-NAMESPACE CLIP (APPUNTI)
' =================================================================================================
' Questo test copre il sub-namespace CLIP che non compariva in alcun esempio .bas:
'   CLIP SET, CLIP GET, CLIP CLEAR.
'
' COPERTURA:
' - CLIP SET espressione : imposta testo negli appunti.
' - CLIP GET destinazione: legge testo dagli appunti.
' - CLIP CLEAR           : svuota gli appunti.
' =================================================================================================
IMPORT CONSOLE AS CS
IMPORT CLIP

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "CONSOLE.CLIP - Funzioni scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    ' 1. CLIP SET
    PRINT "-- CLIP.SET --"
    CLIP SET "Testo di prova dagli appunti"
    PRINT "Testo impostato."
    PRINT

    ' 2. CLIP GET
    PRINT "-- CLIP.GET --"
    DIM testo AS STRING
    CLIP GET testo
    PRINT "Letto: "; testo
    PRINT

    ' 3. CLIP CLEAR
    PRINT "-- CLIP CLEAR --"
    CLIP CLEAR
    CLIP GET testo
    IF LEN(testo) = 0 THEN
        PRINT "Appunti svuotati correttamente."
    ELSE
        PRINT "Appunti ancora pieni: '"; testo; "'"
    END IF
    PRINT

    PRINT "Test Completato."
END SUB