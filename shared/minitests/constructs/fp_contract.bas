' MiniTest: FP contraction (K6) — `a*b + c` NON deve essere contratto in FMA.
' Con FMA (a*b esatto, poi +c) il risultato sarebbe 1; con mul+add in double e' 0.
' Parita' 4 backend: tutti devono stampare 0 (golden bit-exact, flag -ffp-contract=off).
' EXPECT: fma= 0
SUB Main()
    DIM a AS DOUBLE
    DIM b AS DOUBLE
    DIM c AS DOUBLE
    a = 134217729.0
    b = 134217729.0
    c = -18014398777917440.0
    PRINT "fma="; a * b + c
END SUB
