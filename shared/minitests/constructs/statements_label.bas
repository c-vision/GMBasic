' EXPECT: prima|dopo-label|fine
SUB Main()
    PRINT "prima"
    GOTO Salto
Errore:
    PRINT "errore"
Salto:
    PRINT "dopo-label"
    GOTO Fine
Fine:
    PRINT "fine"
END SUB
