' EXPECT: c0= 11|c1= 22|c2= 33
' Array BYREF multiplo (3 array nella stessa chiamata, non tutti in posizione 0 —
' n e' il primo parametro, scalare): out(i) = a(i) + b(i). Copre arrayMask
' multi-bit con piu' array attivi insieme.
DECLARE SUB VecCombine LIB "libvec_module" ALIAS "vec_combine" (n AS LONG, a() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE)

SUB Main()
    DIM a(3) AS DOUBLE
    DIM b(3) AS DOUBLE
    DIM c(3) AS DOUBLE
    a(0) = 1 : a(1) = 2 : a(2) = 3
    b(0) = 10 : b(1) = 20 : b(2) = 30
    VecCombine(3, a, b, c)
    PRINT "c0="; c(0)
    PRINT "c1="; c(1)
    PRINT "c2="; c(2)
END SUB
