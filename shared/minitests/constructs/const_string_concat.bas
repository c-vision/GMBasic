' CONST stringa usata in un'espressione: la costante e' una STRINGA (con o senza AS STRING), quindi `+` concatena.
' Su C99Native la costante veniva registrata come numero e `P + "y"` dava "0y" invece di "x/y".
' EXPECT: a=[abc/1] b=[xyz/2]|abc/q len= 4|n= 5  m= 10
CONST A = "abc/"
CONST B AS STRING = "xyz/"
CONST N = 5

SUB Main()
    PRINT "a=["; A + "1"; "] b=["; B + "2"; "]"
    DIM s AS STRING
    s = A + "q"
    PRINT s; " len="; LEN(A)
    PRINT "n="; N; " m="; N * 2
END SUB
