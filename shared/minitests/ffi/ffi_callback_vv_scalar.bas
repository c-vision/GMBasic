' EXPECT: dot= 11
' Callback con DUE ARRAY in ingresso e ritorno SCALARE (2026-11-02, descriptor 129):
' f(a(), b()) AS DOUBLE. Native: double (*)(double*, int64_t, double*, int64_t).
DECLARE FUNCTION VecCbVvScalar LIB "libvec_module" ALIAS "vec_cb_vv_scalar" (cb AS FUNCTION(a() AS DOUBLE, b() AS DOUBLE) AS DOUBLE, a() AS DOUBLE, na AS LONG, b() AS DOUBLE, nb AS LONG) AS DOUBLE

SUB Main()
    DIM a(1) AS DOUBLE
    a(0) = 1.0 : a(1) = 2.0
    DIM b(1) AS DOUBLE
    b(0) = 3.0 : b(1) = 4.0
    PRINT "dot="; VecCbVvScalar(FN(u() AS DOUBLE, v() AS DOUBLE) AS DOUBLE => u(0) * v(0) + u(1) * v(1), a, 2, b, 2)
END SUB
