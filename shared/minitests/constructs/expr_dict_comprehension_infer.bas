' Comprehension di dict/set senza tipo esplicito (`DIM d = { k : v FOR ... }`): il Linker non le inferiva (la variabile restava
' INFERRED → emitter C# la dichiarava double, C99Native non la riconosceva come dict: "funzione 'd' non definita").
' EXPECT: n= 3|v3= 9|big= 2|s= 3|p= 2
SUB Main()
    DIM d = { x : x*x FOR x IN {1, 2, 3} }
    PRINT "n="; d.Count
    PRINT "v3="; d(3)
    DIM big = { x : x FOR x IN {1, 5, 7, 2} IF x > 4 }
    PRINT "big="; big.Count
    DIM s = { x MOD 5 FOR x IN {1, 2, 6, 11, 4} }
    PRINT "s="; s.Count
    DIM p = { x : y FOR x IN {1, 2} FOR y IN {10, 20} }
    PRINT "p="; p.Count
END SUB
