' EXPECT: c0= 2 |c1= 4 |c2= 6
' Callback con DUE ARRAY in ingresso e RITORNO ARRAY (2026-11-02): f(p[], x[]) AS ARRAY
' — forma del modello di OPTIMIZE.LEVENBERGMARQUARDTVEC. Native:
' void (*)(double* p, int64_t np, double* x, int64_t nx, double* out).
' Nota: la lunghezza dell'array di uscita coincide con quella dell'ULTIMO array in ingresso.
DECLARE SUB VecCbVvOut LIB "libvec_module" ALIAS "vec_cb_vv_out" (cb AS FUNCTION(p() AS DOUBLE, x() AS DOUBLE) AS ARRAY, p() AS DOUBLE, x() AS DOUBLE, out() AS DOUBLE)

SUB Main()
    DIM p(0) AS DOUBLE
    p(0) = 2.0
    DIM x(2) AS DOUBLE
    x(0) = 1.0 : x(1) = 2.0 : x(2) = 3.0
    DIM o(2) AS DOUBLE
    VecCbVvOut(FN(w() AS DOUBLE, z() AS DOUBLE) AS ARRAY OF DOUBLE => {w(0) * z(0), w(0) * z(1), w(0) * z(2)}, p, x, o)
    PRINT "c0="; o(0); "|c1="; o(1); "|c2="; o(2)
END SUB
