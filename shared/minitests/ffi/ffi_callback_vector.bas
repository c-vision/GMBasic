' EXPECT: ss= 25|ss2= 13
' Callback VETTORIALE (2026-11-02): `FUNCTION(v() AS DOUBLE) AS DOUBLE` come parametro FFI.
' Il nativo lo riceve come (double* v, int64_t n); il trampolino ricostruisce l'array BASIC.
DECLARE FUNCTION VecCbVecD LIB "libvec_module" ALIAS "vec_cb_vecd" (cb AS FUNCTION(v() AS DOUBLE) AS DOUBLE, buf() AS DOUBLE, n AS LONG) AS DOUBLE

SUB Main()
    DIM v(1) AS DOUBLE
    v(0) = 3.0 : v(1) = 4.0
    PRINT "ss="; VecCbVecD(FN(w() AS DOUBLE) AS DOUBLE => w(0) * w(0) + w(1) * w(1), v, 2)
    DIM u(1) AS DOUBLE
    u(0) = 2.0 : u(1) = 3.0
    PRINT "ss2="; VecCbVecD(FN(w() AS DOUBLE) AS DOUBLE => w(0) * w(0) + w(1) * w(1), u, 2)
END SUB
