' =============================================================================
' ClipperGetValidControls.bas — VALID predicato + controlli TYPE
' (Spinner / CheckBox / ComboBox) via @ ... GET ... READ
' =============================================================================

SUB Main()
    DIM eta AS DOUBLE
    DIM qta AS DOUBLE
    DIM accetto AS STRING
    DIM colore AS STRING
    DIM colori(2) AS STRING
    colori(0) = "Rosso"
    colori(1) = "Verde"
    colori(2) = "Blu"

    CLS
    @ 2, 2 SAY "Eta (18-100):"
    @ 2, 16 GET eta VALID eta >= 18 AND eta <= 100
    @ 3, 2 SAY "Qta (0-99, step 5):"
    @ 3, 21 GET qta TYPE "Spinner" RANGE 0, 99 STEP 5
    @ 4, 2 SAY "Accetto:"
    @ 4, 10 GET accetto TYPE "CheckBox" CAPTION "Accetto i termini"
    @ 5, 2 SAY "Colore:"
    @ 5, 10 GET colore TYPE "ComboBox" ITEMS colori SIZE 12
    READ
    PRINT "VALID_CONTROLS_OK"
    PRINT "ETA="; eta
    PRINT "QTA="; qta
    PRINT "ACCETTO="; accetto
    PRINT "COLORE="; colore
END SUB
