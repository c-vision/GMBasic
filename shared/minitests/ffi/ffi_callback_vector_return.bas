' EXPECT: a0= 6|a1= 8|b0= 3|b1= 4
' Callback con RITORNO ARRAY (2026-11-02): f(v[]) AS ARRAY — forma di Fsolve/Broyden/LSQFit.
DECLARE SUB VecCbArrOut LIB "libvec_module" ALIAS "vec_cb_arr_out" (cb AS FUNCTION(v() AS DOUBLE) AS ARRAY, buf() AS DOUBLE, out() AS DOUBLE)

SUB Main()
    DIM v(1) AS DOUBLE
    v(0) = 3.0 : v(1) = 4.0
    DIM o(1) AS DOUBLE
    VecCbArrOut(FN(w() AS DOUBLE) AS ARRAY OF DOUBLE => {w(0) * 2.0, w(1) * 2.0}, v, o)
    PRINT "a0="; o(0)
    PRINT "a1="; o(1)
    DIM q(1) AS DOUBLE
    VecCbArrOut(FN(w() AS DOUBLE) AS ARRAY OF DOUBLE => {w(0), w(1)}, v, q)
    PRINT "b0="; q(0)
    PRINT "b1="; q(1)
END SUB
