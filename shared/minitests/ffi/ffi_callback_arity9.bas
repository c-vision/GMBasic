' MiniTest FFI (2026-11-11): callback con ARITÀ 9 (> 8, il vecchio limite dei pool di
' trampolini pre-compilati). Il trampolino ha la firma esatta: nove `double` → `double`.
' EXPECT: a= 4.5
DECLARE FUNCTION CbApply9 LIB "liblayout_ext" ALIAS "cb_apply9" (f AS FUNCTION(a AS DOUBLE, b AS DOUBLE, c AS DOUBLE, d AS DOUBLE, e AS DOUBLE, f AS DOUBLE, g AS DOUBLE, h AS DOUBLE, i AS DOUBLE) AS DOUBLE, x AS DOUBLE) AS DOUBLE

SUB Main()
    ' 9 * 0.5 = 4.5
    PRINT "a="; CbApply9(FN(a AS DOUBLE, b AS DOUBLE, c AS DOUBLE, d AS DOUBLE, e AS DOUBLE, f AS DOUBLE, g AS DOUBLE, h AS DOUBLE, i AS DOUBLE) AS DOUBLE => (a + b + c + d + e + f + g + h + i) * 0.5, 1.0)
END SUB
