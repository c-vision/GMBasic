' FILE: Samples/Clipper/ClipperDialogs.bas
' Test ALERT e CONFIRM

SUB Main()
    PRINT "=== Dialog Test ==="
    
    ' ALERT semplice
    DIM r = ALERT("Benvenuto in GMBasic!")
    PRINT "Alert mostrato"
    
    ' CONFIRM con risultato
    DIM risposta AS BOOLEAN
    risposta = CONFIRM("Continuare con l'operazione?") = %OK
    
    IF risposta THEN
        PRINT "Hai scelto SI"
    ELSE
        PRINT "Hai scelto NO"
    END IF
    
    PRINT "Test completato"
END SUB
