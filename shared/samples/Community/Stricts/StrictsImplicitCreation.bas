' FILE: Stricts/StrictsImplicitCreation.bas
' TEST: Divieto di Creazione Implicita di Variabili
' =================================================================
' Con OPTION STRICT ON, ogni variabile deve essere dichiarata
' esplicitamente (DIM, LOCAL, etc.) prima di essere utilizzata.
' L'assegnazione a una variabile non dichiarata (implicitVar)
' è una violazione.
' =================================================================
OPTION STRICT OFF
SUB Main()
    PRINT "Test: Divieto di Creazione Implicita"
    
    ' VIOLATION: Assegnazione a una variabile non dichiarata.
    implicitVar = 100
    
    PRINT "FAIL: La compilazione doveva fallire."
END SUB
