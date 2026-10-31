' MiniTest STATEMENTS.SWAP — comando SWAP (scambio di due variabili).
' Parità IL/PCode/VM-C/C99Native. Oracolo: backfill IL.
' EXPECT: num= 2 , 1|str=beta,alpha|glob= 20 , 10|mix= 99 , 7|expr= 83
DIM gA AS DOUBLE
DIM gB AS DOUBLE

FUNCTION Inc(x AS DOUBLE) AS DOUBLE
    Inc = x + 1
END FUNCTION

SUB Main()
    ' Scalari numerici (double)
    a = 1 : b = 2
    SWAP a, b
    PRINT "num="; a; ","; b
    ' Stringhe
    s1$ = "alpha" : s2$ = "beta"
    SWAP s1$, s2$
    PRINT "str="; s1$; ","; s2$
    ' Globali
    gA = 10 : gB = 20
    SWAP gA, gB
    PRINT "glob="; gA; ","; gB
    ' Locale <-> globale
    c = 7 : gA = 99
    SWAP c, gA
    PRINT "mix="; c; ","; gA
    ' Usa il risultato in espressione
    d = 3 : e = 8
    SWAP d, e
    PRINT "expr="; d * 10 + e
END SUB
