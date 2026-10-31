' FILE: Stricts/StrictsTypelessDim.bas
' TEST: Obbligo di Tipo Esplicito per le Dichiarazioni (DIM)
' =================================================================
' Con OPTION STRICT ON, ogni dichiarazione DIM deve specificare un
' tipo di dato, o tramite la clausola 'AS Tipo' o tramite un
' suffisso di tipo (es. miaVariabile$). Dichiarare una variabile
' senza specificarne il tipo è una violazione.
' =================================================================
OPTION STRICT OFF
SUB Main()
    PRINT "Test: Obbligo di Tipo Esplicito per DIM"
    
    ' VIOLATION: Dichiarazione di variabile senza tipo esplicito.
    DIM typelessVar
    
    PRINT "FAIL: La compilazione doveva fallire."
END SUB
