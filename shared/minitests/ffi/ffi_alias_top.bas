' EXPECT: scaled= 20
DECLARE FUNCTION ScaleDouble LIB "libvec_module" ALIAS "vec_scale" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE

SUB Main()
    PRINT "scaled="; ScaleDouble(4, 5)
END SUB
