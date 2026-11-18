' EXPECT: dentro try= 7|catch= 11|tryvar= 0|finally= 1
' TRY/CATCH/FINALLY: ogni clausola ha uno scope INDIPENDENTE (spec Variables-Scope §6.3).
' - una variabile dichiarata nel TRY (TryVar) vive solo lì: letta nel CATCH non è in scope e vale
'   il default del tipo (0), non il valore che aveva nel TRY;
' - la variabile dell'handler (ex) è locale alla clausola CATCH;
' - una variabile esterna (Handle) resta accessibile in FINALLY.
SUB ProvaTry()
    DIM Handle AS LONG
    Handle = 1
    TRY
        DIM TryVar AS LONG
        TryVar = 7
        PRINT "dentro try="; TryVar
        ERROR 11
    CATCH ex AS ERR
        PRINT "catch="; ex.Number
        PRINT "tryvar="; TryVar
    FINALLY
        PRINT "finally="; Handle
    END TRY
END SUB

SUB Main()
    ProvaTry()
END SUB
