' FILE: Stricts/StrictsIncompatibleOp.bas
' TEST: Divieto di Operazioni tra Tipi Incompatibili
' =================================================================
' Con OPTION STRICT ON, non sono permesse operazioni aritmetiche
' (come l'addizione '+') tra tipi incompatibili, come un numero
' e una stringa. Questo previene la coercizione implicita che
' avverrebbe in modalità flessibile.
' =================================================================
OPTION STRICT OFF
SUB Main()
    PRINT "Test: Divieto di Operazioni tra Tipi Incompatibili"
    
    ' VIOLATION: Tentativo di sommare un numero e una stringa.
    DIM risultato AS STRING = 10 + " apples"
    
    PRINT "FAIL: La compilazione doveva fallire."
END SUB
