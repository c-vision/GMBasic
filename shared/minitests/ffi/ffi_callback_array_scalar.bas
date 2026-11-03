' EXPECT: r= 14
' Callback ARRAY + SCALARE (2026-11-02): forma dei modelli LM/CurveFit f(p[], x).
' Native: double (*)(double* p, int64_t n, double x).
DECLARE FUNCTION VecCbArrScalar LIB "libvec_module" ALIAS "vec_cb_arr_scalar" (cb AS FUNCTION(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE, buf() AS DOUBLE, n AS LONG, s AS DOUBLE) AS DOUBLE

SUB Main()
    DIM v(1) AS DOUBLE
    v(0) = 3.0 : v(1) = 4.0
    PRINT "r="; VecCbArrScalar(FN(w() AS DOUBLE, s AS DOUBLE) AS DOUBLE => (w(0) + w(1)) * s, v, 2, 2.0)
END SUB
