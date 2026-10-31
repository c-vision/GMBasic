' ASHUFFLE su array STRING: rimescola in place preservando tutti gli elementi (l'ordine
' e' casuale: il test verifica cardinalita' e integrita', non la permutazione).
' EXPECT: count= 4|len= 4
IMPORT ARRAYS
IMPORT STRINGS
SUB Main()
    DIM s$() = {"D", "A", "C", "B"}
    ASHUFFLE s$
    PRINT "count="; UBOUND(s$) + 1
    PRINT "len="; LEN(JOIN(s$, ""))
END SUB
