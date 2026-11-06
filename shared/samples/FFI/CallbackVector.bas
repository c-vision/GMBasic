REM FILE: CallbackVector.bas
' FFI (2026-11-02): callback VETTORIALE `FUNCTION(v() AS DOUBLE) AS DOUBLE` come parametro.
' Il nativo lo riceve come (double* v, int64_t n); il trampolino ricostruisce l'array BASIC.
' Stato: PASS sui 4 backend (IL, PCode, VM/C, C99Native) — il bound dell'array viene
' pubblicato dal trampolino (gmb_lambda_bound). Stesso caso del minitest ffi_callback_vector.
' Uso: ../../tools/sh/parity4 shared/samples/FFI/CallbackVector.bas
DECLARE FUNCTION VecCbVecD LIB "libvec_module" ALIAS "vec_cb_vecd" (cb AS FUNCTION(v() AS DOUBLE) AS DOUBLE, buf() AS DOUBLE) AS DOUBLE

SUB Main()
    DIM v(1) AS DOUBLE
    v(0) = 3.0 : v(1) = 4.0
    PRINT "ss="; VecCbVecD(FN(w() AS DOUBLE) AS DOUBLE => w(0) * w(0) + w(1) * w(1), v)
    DIM u(1) AS DOUBLE
    u(0) = 2.0 : u(1) = 3.0
    PRINT "ss2="; VecCbVecD(FN(w() AS DOUBLE) AS DOUBLE => w(0) * w(0) + w(1) * w(1), u)
END SUB
