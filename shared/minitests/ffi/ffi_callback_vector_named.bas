' EXPECT: r128= 25|r129= 11|r132= 14|r144= 6 , 8|r145= 2 , 4 , 6
' Callback VETTORIALI con FUNCTION NOMINATE (2026-11-02) — tutte le forme:
' 128 f(v) -> scalare, 129 f(a,b) -> scalare, 132 f(p,x) -> scalare,
' 144 f(v) -> array, 145 f(p,x) -> array. Parità fra i 4 backend.
DECLARE FUNCTION VecCbVecD LIB "libvec_module" ALIAS "vec_cb_vecd" (cb AS FUNCTION(v() AS DOUBLE) AS DOUBLE, buf() AS DOUBLE, n AS LONG) AS DOUBLE
DECLARE FUNCTION VecCbVvScalar LIB "libvec_module" ALIAS "vec_cb_vv_scalar" (cb AS FUNCTION(a() AS DOUBLE, b() AS DOUBLE) AS DOUBLE, a() AS DOUBLE, na AS LONG, b() AS DOUBLE, nb AS LONG) AS DOUBLE
DECLARE FUNCTION VecCbArrScalar LIB "libvec_module" ALIAS "vec_cb_arr_scalar" (cb AS FUNCTION(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE, buf() AS DOUBLE, n AS LONG, s AS DOUBLE) AS DOUBLE
DECLARE SUB VecCbArrOut LIB "libvec_module" ALIAS "vec_cb_arr_out" (cb AS FUNCTION(v() AS DOUBLE) AS ARRAY, buf() AS DOUBLE, n AS LONG, out() AS DOUBLE)
DECLARE SUB VecCbVvOut LIB "libvec_module" ALIAS "vec_cb_vv_out" (cb AS FUNCTION(p() AS DOUBLE, x() AS DOUBLE) AS ARRAY, p() AS DOUBLE, np AS LONG, x() AS DOUBLE, nx AS LONG, out() AS DOUBLE)

FUNCTION SommaQuad(w() AS DOUBLE) AS DOUBLE
    RETURN w(0) * w(0) + w(1) * w(1)
END FUNCTION

FUNCTION Prodotto(a() AS DOUBLE, b() AS DOUBLE) AS DOUBLE
    RETURN a(0) * b(0) + a(1) * b(1)
END FUNCTION

FUNCTION Scala(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE
    RETURN (p(0) + p(1)) * x
END FUNCTION

FUNCTION Doppio(v() AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {v(0) * 2.0, v(1) * 2.0}
END FUNCTION

FUNCTION ProdottoVec(p() AS DOUBLE, x() AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {p(0) * x(0), p(0) * x(1), p(0) * x(2)}
END FUNCTION

SUB Main()
    DIM v(1) AS DOUBLE
    v(0) = 3.0 : v(1) = 4.0
    PRINT "r128="; VecCbVecD(SommaQuad, v, 2)

    DIM b(1) AS DOUBLE
    b(0) = 1.0 : b(1) = 2.0
    PRINT "r129="; VecCbVvScalar(Prodotto, v, 2, b, 2)

    PRINT "r132="; VecCbArrScalar(Scala, v, 2, 2.0)

    DIM o(1) AS DOUBLE
    VecCbArrOut(Doppio, v, 2, o)
    PRINT "r144="; o(0); ","; o(1)

    DIM p(0) AS DOUBLE
    p(0) = 2.0
    DIM x(2) AS DOUBLE
    x(0) = 1.0 : x(1) = 2.0 : x(2) = 3.0
    DIM y(2) AS DOUBLE
    VecCbVvOut(ProdottoVec, p, 1, x, 3, y)
    PRINT "r145="; y(0); ","; y(1); ","; y(2)
END SUB
