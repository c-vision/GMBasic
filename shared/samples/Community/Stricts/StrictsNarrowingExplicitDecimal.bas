' FILE: Stricts/StrictsNarrowingExplicitDecimal.bas
' TEST: Divieto di Conversione "Narrowing" Esplicita (Perdita Frazionaria)
' =================================================================
' Con OPTION STRICT ON, anche le funzioni di casting esplicito
' come CINT() generano un errore a tempo di compilazione se vengono
' usate con un valore *letterale* che causerebbe una perdita di dati.
' =================================================================
OPTION STRICT OFF
SUB Main()
    PRINT "Test: Divieto di Conversione Narrowing Esplicita (Decimale)"
    
    ' VIOLATION: Uso di CINT() con un letterale che ha una parte frazionaria.
    DIM i AS INTEGER = CINT(123.45)
    
    PRINT "FAIL: La compilazione doveva fallire."
END SUB
