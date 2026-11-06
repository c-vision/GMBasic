' Callback FFI con 6 parametri (S4, 2026-11-06): oltre il limite v1 di 4.
' EXPECT: r= 91
DECLARE FUNCTION Cb6Eval LIB "liblayout_ext" ALIAS "cb6_eval" (cb AS FUNCTION(a AS DOUBLE, b AS DOUBLE, c AS DOUBLE, d AS DOUBLE, e AS DOUBLE, g AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE, c AS DOUBLE, d AS DOUBLE, e AS DOUBLE, g AS DOUBLE) AS DOUBLE

SUB Main()
    PRINT "r="; Cb6Eval(FN(a AS DOUBLE, b AS DOUBLE, c AS DOUBLE, d AS DOUBLE, e AS DOUBLE, g AS DOUBLE) AS DOUBLE => a + 2*b + 3*c + 4*d + 5*e + 6*g, 1, 2, 3, 4, 5, 6)
END SUB
