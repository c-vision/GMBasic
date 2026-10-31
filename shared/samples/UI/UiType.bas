' FILE: UI/UiType.bas
' =================================================================================================
' UI.TYPE(target$, text$) — digita testo in un elemento UI del CUA
' Richiede una finestra attiva con accessibilità abilitata.
' =================================================================================================
IMPORT UI

SUB Main()
    CLS
    PRINT "=== UI.TYPE ==="
    DIM result = UI.TYPE("input[type=text]", "testo di esempio")
    PRINT "Result: "; result
    PRINT "=== OK ==="
END SUB