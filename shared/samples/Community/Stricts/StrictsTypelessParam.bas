' FILE: Stricts/StrictsTypelessParam.bas
' TEST: Obbligo di Tipo Esplicito per i Parametri di Procedura
' =================================================================
' Con OPTION STRICT ON, ogni parametro in una dichiarazione SUB o
' FUNCTION deve avere un tipo esplicito, specificato con 'AS Tipo'
' o con un suffisso di tipo. Un parametro senza tipo è una violazione.
' =================================================================
OPTION STRICT OFF

' VIOLATION: Parametro 'param' dichiarato senza un tipo esplicito.
SUB SubSenzaTipo(param)
    PRINT param
END SUB

SUB Main()
    PRINT "Test: Obbligo di Tipo Esplicito per i Parametri"
    PRINT "FAIL: La compilazione doveva fallire."
END SUB
