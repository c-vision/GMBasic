REM FILE: CallbackVectorReturn.bas
' FFI (2026-11-02): callback con RITORNO ARRAY.
'   f(v()) AS ARRAY        -> void (*)(double*, int64_t, double*)
'   f(p(), x()) AS ARRAY    -> void (*)(double*, int64_t, double*, int64_t, double*)
' Sono le forme di OPTIMIZE.FSOLVE / BROYDEN / LEASTSQUARESFIT / LEVENBERGMARQUARDTVEC.
DECLARE SUB VecCbArrOut LIB "libvec_module" ALIAS "vec_cb_arr_out" (cb AS FUNCTION(v() AS DOUBLE) AS ARRAY, buf() AS DOUBLE, n AS LONG, out() AS DOUBLE)
DECLARE FUNCTION VecCbVvScalar LIB "libvec_module" ALIAS "vec_cb_vv_scalar" (cb AS FUNCTION(a() AS DOUBLE, b() AS DOUBLE) AS DOUBLE, a() AS DOUBLE, na AS LONG, b() AS DOUBLE, nb AS LONG) AS DOUBLE
DECLARE SUB VecCbVvOut LIB "libvec_module" ALIAS "vec_cb_vv_out" (cb AS FUNCTION(p() AS DOUBLE, x() AS DOUBLE) AS ARRAY, p() AS DOUBLE, np AS LONG, x() AS DOUBLE, nx AS LONG, out() AS DOUBLE)

SUB Main()
    ' f(v) = 2*v
    DIM v(1) AS DOUBLE
    v(0) = 3.0 : v(1) = 4.0
    DIM o(1) AS DOUBLE
    VecCbArrOut(FN(w() AS DOUBLE) AS ARRAY OF DOUBLE => {w(0) * 2.0, w(1) * 2.0}, v, 2, o)
    PRINT "doppio="; o(0); ","; o(1)

    ' f(p, x) = p(0)*x
    DIM p(0) AS DOUBLE
    p(0) = 2.0
    DIM x(2) AS DOUBLE
    x(0) = 1.0 : x(1) = 2.0 : x(2) = 3.0
    DIM y(2) AS DOUBLE
    VecCbVvOut(FN(w() AS DOUBLE, z() AS DOUBLE) AS ARRAY OF DOUBLE => {w(0) * z(0), w(0) * z(1), w(0) * z(2)}, p, 1, x, 3, y)
    PRINT "prodotto="; y(0); ","; y(1); ","; y(2)

    ' prodotto scalare con due array in ingresso
    PRINT "dot="; VecCbVvScalar(FN(u() AS DOUBLE, w() AS DOUBLE) AS DOUBLE => u(0) * w(0) + u(1) * w(1), v, 2, y, 2)
END SUB

' Forma 2 array -> scalare (descriptor 129): f(a(), b()) AS DOUBLE
