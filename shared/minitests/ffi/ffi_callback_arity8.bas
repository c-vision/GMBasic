' Callback FFI a 8 parametri (double) e 6 parametri (interi) — S4, oltre il limite v1 di 4.
' EXPECT: r8= 204|ri= 21
DECLARE FUNCTION Cb8Eval LIB "liblayout_ext" ALIAS "cb8_eval" (cb AS FUNCTION(a AS DOUBLE, b AS DOUBLE, c AS DOUBLE, d AS DOUBLE, e AS DOUBLE, g AS DOUBLE, h AS DOUBLE, i AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE, c AS DOUBLE, d AS DOUBLE, e AS DOUBLE, g AS DOUBLE, h AS DOUBLE, i AS DOUBLE) AS DOUBLE
DECLARE FUNCTION Cb6EvalI LIB "liblayout_ext" ALIAS "cb6_eval_i" (cb AS FUNCTION(a AS LONG, b AS LONG, c AS LONG, d AS LONG, e AS LONG, g AS LONG) AS LONG, a AS LONG, b AS LONG, c AS LONG, d AS LONG, e AS LONG, g AS LONG) AS LONG

SUB Main()
    PRINT "r8="; Cb8Eval(FN(a AS DOUBLE, b AS DOUBLE, c AS DOUBLE, d AS DOUBLE, e AS DOUBLE, g AS DOUBLE, h AS DOUBLE, i AS DOUBLE) AS DOUBLE => a + 2*b + 3*c + 4*d + 5*e + 6*g + 7*h + 8*i, 1, 2, 3, 4, 5, 6, 7, 8)
    PRINT "ri="; Cb6EvalI(FN(a AS LONG, b AS LONG, c AS LONG, d AS LONG, e AS LONG, g AS LONG) AS LONG => a + b + c + d + e + g, 1, 2, 3, 4, 5, 6)
END SUB
