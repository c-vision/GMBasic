' 3-D + copy-back in place (scale3): dopo la chiamata il cubo deve risultare scalato.
' EXPECT: s3= 36|k0= 2|k7= 16|s3b= 72
DECLARE FUNCTION SUM3I  LIB "libdims_ext" ALIAS "sum3"   (a() AS DOUBLE) AS DOUBLE
DECLARE SUB      SCALE3I LIB "libdims_ext" ALIAS "scale3" (a() AS DOUBLE, k AS DOUBLE)

SUB Main()
    DIM c(1, 1, 1) AS DOUBLE
    DIM v AS DOUBLE
    v = 1
    FOR i = 0 TO 1
        FOR j = 0 TO 1
            FOR k = 0 TO 1
                c(i, j, k) = v
                v = v + 1
            NEXT k
        NEXT j
    NEXT i
    PRINT "s3="; SUM3I(c)
    SCALE3I(c, 2)
    PRINT "k0="; c(0, 0, 0)
    PRINT "k7="; c(1, 1, 1)
    PRINT "s3b="; SUM3I(c)
END SUB
