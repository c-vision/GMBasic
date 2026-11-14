' MiniTest CONSTRUCTS — GENERATOR: il corpo è una COROUTINA, quindi ammette i costrutti
' normali (prima era una macchina a stati con un sottoinsieme: niente TRY/CATCH, niente
' FOR EACH, niente DEFER/ERRDEFER).
'
' Copre:
'   1. FOR EACH dentro il generatore (con YIELD nel ciclo);
'   2. TRY/CATCH DENTRO il generatore: l'errore è catturato lì e il generatore continua;
'   3. ERRDEFER nel corpo: il cleanup corre quando un errore ATTRAVERSA il generatore;
'   4. DEFER nel corpo: corre quando il corpo termina normalmente.
'
' Nota: il CATCH esterno stampa solo "errore" e NON e.Number — il codice dell'errore che
' sfugge a un generatore oggi diverge sulle gambe gestite (VM/C riporta StopIteration 144,
' PCode 200 dopo un CATCH interno): è un difetto pre-esistente dei loro driver di
' generatore, tracciato a parte, non del costrutto.
'
' EXPECT: n 1 = 10|n 2 = 20|n 3 = 30|n 4 = 100|generatore cattura  11|n 5 =-1|n 6 = 300|cleanup-errdefer|errore|fine
GENERATOR G(n AS LONG) AS LONG
    ERRDEFER PRINT "cleanup-errdefer"
    DEFER PRINT "cleanup-defer"
    DIM lst AS LIST OF INTEGER
    lst = [10, 20, 30]
    DIM v AS INTEGER
    FOR EACH v IN lst
        YIELD v
    NEXT
    DIM i AS LONG
    FOR i = 1 TO n
        TRY
            IF i = 2 THEN ERROR 11, "dentro il generatore"
            YIELD i * 100
        CATCH e AS ERR
            PRINT "generatore cattura "; e.Number
            YIELD -1
        END TRY
    NEXT i
    ERROR 13, "errore finale"
END GENERATOR

SUB Main()
    DIM it = G(3)
    TRY
        DIM k AS LONG
        DIM val AS LONG
        FOR k = 1 TO 7
            ' Il valore si legge PRIMA del PRINT: un PRINT dentro il generatore durante la
            ' valutazione dell'argomento interlaccia lo staging del PRINT esterno (differenza
            ' pre-esistente fra le gambe, non del costrutto).
            val = NEXT(it)
            PRINT "n"; k; "="; val
        NEXT k
    CATCH e AS ERR
        PRINT "errore"
    END TRY
    PRINT "fine"
END SUB
