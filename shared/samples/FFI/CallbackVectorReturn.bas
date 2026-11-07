REM FILE: CallbackVectorReturn.bas
' FFI (2026-11-02 / 2026-11-07): callback con RITORNO ARRAY.
'   f(v()) AS ARRAY             -> void (*)(double*, int64_t, double*)
'   f(p(), x()) AS ARRAY        -> void (*)(double*, int64_t, double*, int64_t, double*)
'   f(v(), s) AS ARRAY          -> void (*)(double*, int64_t, double, double*)   [148]
'   f(t) AS ARRAY               -> double (*)(double, double*)                   [193]
'   f(u, v) AS ARRAY            -> double (*)(double, double, double*)           [194]
'   f(x, y, z) AS ARRAY         -> double (*)(double, double, double, double*)   [195]
' Le prime due sono le forme di OPTIMIZE.FSOLVE / BROYDEN / LEASTSQUARESFIT /
' LEVENBERGMARQUARDTVEC; le altre di OPTIMIZE e di VECCALC (campi dati come formule).
DECLARE SUB VecCbArrOut LIB "libvec_module" ALIAS "vec_cb_arr_out" (cb AS FUNCTION(v() AS DOUBLE) AS ARRAY, buf() AS DOUBLE, out() AS DOUBLE)
DECLARE FUNCTION VecCbVvScalar LIB "libvec_module" ALIAS "vec_cb_vv_scalar" (cb AS FUNCTION(a() AS DOUBLE, b() AS DOUBLE) AS DOUBLE, a() AS DOUBLE, b() AS DOUBLE) AS DOUBLE
DECLARE SUB VecCbVvOut LIB "libvec_module" ALIAS "vec_cb_vv_out" (cb AS FUNCTION(p() AS DOUBLE, x() AS DOUBLE) AS ARRAY, p() AS DOUBLE, x() AS DOUBLE, out() AS DOUBLE)
DECLARE SUB VecCbArrSOut LIB "libvec_module" ALIAS "vec_cb_arr_s_out" (cb AS FUNCTION(v() AS DOUBLE, s AS DOUBLE) AS ARRAY, buf() AS DOUBLE, s AS DOUBLE, out() AS DOUBLE)
DECLARE SUB VecCbS1Out LIB "libvec_module" ALIAS "vec_cb_s1_out" (cb AS FUNCTION(t AS DOUBLE) AS ARRAY, t AS DOUBLE, out() AS DOUBLE)
DECLARE SUB VecCbS2Out LIB "libvec_module" ALIAS "vec_cb_s2_out" (cb AS FUNCTION(u AS DOUBLE, v AS DOUBLE) AS ARRAY, u AS DOUBLE, v AS DOUBLE, out() AS DOUBLE)
DECLARE SUB VecCbS3Out LIB "libvec_module" ALIAS "vec_cb_s3_out" (cb AS FUNCTION(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY, x AS DOUBLE, y AS DOUBLE, z AS DOUBLE, out() AS DOUBLE)

SUB Main()
    ' f(v) = 2*v
    DIM v(1) AS DOUBLE
    v(0) = 3.0 : v(1) = 4.0
    DIM o(1) AS DOUBLE
    VecCbArrOut(FN(w() AS DOUBLE) AS ARRAY OF DOUBLE => {w(0) * 2.0, w(1) * 2.0}, v, o)
    PRINT "doppio="; o(0); ","; o(1)

    ' f(p, x) = p(0)*x
    DIM p(0) AS DOUBLE
    p(0) = 2.0
    DIM x(2) AS DOUBLE
    x(0) = 1.0 : x(1) = 2.0 : x(2) = 3.0
    DIM y(2) AS DOUBLE
    VecCbVvOut(FN(w() AS DOUBLE, z() AS DOUBLE) AS ARRAY OF DOUBLE => {w(0) * z(0), w(0) * z(1), w(0) * z(2)}, p, x, y)
    PRINT "prodotto="; y(0); ","; y(1); ","; y(2)

    ' prodotto scalare con due array in ingresso
    PRINT "dot="; VecCbVvScalar(FN(u() AS DOUBLE, w() AS DOUBLE) AS DOUBLE => u(0) * w(0) + u(1) * w(1), v, y)

    ' ── 148: array + scalare -> array (scala un vettore) ──────────────────────
    DIM s2(1) AS DOUBLE
    VecCbArrSOut(FN(w() AS DOUBLE, k AS DOUBLE) AS ARRAY OF DOUBLE => {w(0) * k, w(1) * k}, v, 2.5, s2)
    PRINT "scalato="; s2(0); ","; s2(1)

    ' ── 193: curva r(t) = (t, 2t, t^2) ────────────────────────────────────────
    DIM c3(2) AS DOUBLE
    VecCbS1Out(FN(t AS DOUBLE) AS ARRAY OF DOUBLE => {t, t * 2.0, t * t}, 3.0, c3)
    PRINT "curva="; c3(0); ","; c3(1); ","; c3(2)

    ' ── 194: patch S(u,v) = (u, v, u*v) ───────────────────────────────────────
    DIM s3(2) AS DOUBLE
    VecCbS2Out(FN(u AS DOUBLE, vv AS DOUBLE) AS ARRAY OF DOUBLE => {u, vv, u * vv}, 2.0, 5.0, s3)
    PRINT "patch="; s3(0); ","; s3(1); ","; s3(2)

    ' ── 195: campo F(x,y,z) = (x+y, y+z, x*z) — la forma di VECCALC ───────────
    DIM f3(2) AS DOUBLE
    VecCbS3Out(FN(a AS DOUBLE, b AS DOUBLE, c AS DOUBLE) AS ARRAY OF DOUBLE => {a + b, b + c, a * c}, 1.0, 2.0, 3.0, f3)
    PRINT "campo="; f3(0); ","; f3(1); ","; f3(2)
END SUB

' Forma 2 array -> scalare (descriptor 129): f(a(), b()) AS DOUBLE
