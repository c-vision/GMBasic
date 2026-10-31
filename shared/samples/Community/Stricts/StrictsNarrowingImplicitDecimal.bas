' FILE: Stricts/StrictsNarrowingImplicitDecimal.bas
' TEST: Divieto di Conversione "Narrowing" Implicita (Perdita Frazionaria)
' =================================================================
' Con OPTION STRICT ON, non è permessa l'assegnazione implicita di un
' valore in virgola mobile (Double) a una variabile di tipo intero,
' perché causerebbe la perdita della parte decimale.
' =================================================================
OPTION STRICT OFF
SUB Main()
    PRINT "Test: Divieto di Conversione Narrowing Implicita (Decimale)"
    
    ' VIOLATION: Assegnazione di un letterale Double a un Integer.
    DIM i AS INTEGER = 3.14
    
    PRINT "FAIL: La compilazione doveva fallire."
END SUB
