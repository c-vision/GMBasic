' Regressione (2026-11-04): uno SCALARE omonimo di un builtin non deve shadowarlo,
' mentre una variabile NON TIPIZZATA che contiene una collezione (DIM voci +
' STRINGS.SPLIT) deve continuare a shadoware il builtin omonimo.
' EXPECT: n= 3|10,30|scalar= 5  builtin= 4
SUB Main()
    DIM voci
    voci = STRINGS.SPLIT("10,20,30", ",")
    PRINT "n="; UBOUND(voci) + 1
    PRINT voci(0); ","; voci(2)

    DIM amax = 5
    DIM v = {3, 1, 4}
    PRINT "scalar="; amax; " builtin="; AMAX(v)
END SUB
