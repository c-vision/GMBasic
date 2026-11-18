' MiniTest EXCEPTIONS — stack trace attraverso un PARK (AWAIT) e dentro un ciclo.
'
' Perché: la catena delle procedure deve restare corretta anche quando la procedura
' attraversa un punto di sospensione (AWAIT) e quando l'errore nasce dentro un ciclo:
' i frame non devono "sparire" né duplicarsi.
'
' EXPECT: con park:|in Interna (riga 11)|in ConPark (riga 16)|in Main (riga 29)|in ciclo:|in Interna (riga 11)|in InCiclo (riga 22)|in Main (riga 35)|fine
IMPORT CONCURRENCY

SUB Interna()
    ERROR 11, "boom"
END SUB

SUB ConPark()
    AWAIT CONCURRENCY.TASKYIELD()
    Interna()
END SUB

SUB InCiclo(n AS LONG)
    DIM i AS LONG
    FOR i = 1 TO n
        IF i = n THEN Interna()
    NEXT i
END SUB

SUB Main()
    PRINT "con park:"
    TRY
        ConPark()
    CATCH e AS ERR
        PRINT e.StackTrace
    END TRY
    PRINT "in ciclo:"
    TRY
        InCiclo(3)
    CATCH e AS ERR
        PRINT e.StackTrace
    END TRY
    PRINT "fine"
END SUB
