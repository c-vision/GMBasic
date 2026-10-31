' EXPECT: avg= 5|sum8= 36
' Multi-argomento: 4 argomenti DOUBLE e 8 argomenti LONG (limite massimo FFI ≤ 8).
DECLARE FUNCTION avg4 LIB "libvec_module" ALIAS "vec_avg4" (a AS DOUBLE, b AS DOUBLE, c AS DOUBLE, d AS DOUBLE) AS DOUBLE
DECLARE FUNCTION sum8 LIB "libvec_module" ALIAS "vec_sum8" (a AS LONG, b AS LONG, c AS LONG, d AS LONG, e AS LONG, f AS LONG, g AS LONG, h AS LONG) AS LONG

SUB Main()
    PRINT "avg="; avg4(2, 4, 6, 8)
    PRINT "sum8="; sum8(1, 2, 3, 4, 5, 6, 7, 8)
END SUB
