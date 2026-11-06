' EXPECT: o0= 10|o1= 10
DECLARE SUB VecFive LIB "libvec_module" ALIAS "vec_five2" (a() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, d() AS DOUBLE, out() AS DOUBLE)
SUB Main()
    DIM a(1) AS DOUBLE
    DIM b(1) AS DOUBLE
    DIM c(1) AS DOUBLE
    DIM d(1) AS DOUBLE
    DIM o(1) AS DOUBLE
    a(0) = 1 : a(1) = 1
    b(0) = 2 : b(1) = 2
    c(0) = 3 : c(1) = 3
    d(0) = 4 : d(1) = 4
    CALL VecFive(a, b, c, d, o)
    PRINT "o0="; o(0)
    PRINT "o1="; o(1)
END SUB
