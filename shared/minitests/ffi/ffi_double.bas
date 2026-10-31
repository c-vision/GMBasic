' EXPECT: mean= 5.25|sum3= 9.8
DECLARE FUNCTION vec_scale LIB "libvec_module" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE

SUB Main()
    PRINT "mean="; vec_scale(3.5, 1.5)
    PRINT "sum3="; vec_scale(vec_scale(2.45, 2), 2)
END SUB
