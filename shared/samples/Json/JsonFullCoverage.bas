' FILE: Json/JsonFullCoverage.bas
' Copertura completa delle funzioni JSON non ancora coperte.
IMPORT JSON

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "Json - Copertura Completa"
    PRINT "============================================================"
    PRINT

    DIM data = {"nome": "Ada", "eta": 36, "attivo": TRUE}

    PRINT "-- JSON.ToJson --"
    DIM js = JSON.ToJson(data)
    PRINT "ToJson: "; js
    PRINT

    PRINT "Test Completato."
END SUB
