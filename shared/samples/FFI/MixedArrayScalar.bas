REM FILE: MixedArrayScalar.bas
' FFI (2026-11-02): array BYREF + scalari DOUBLE nella stessa firma.
' Il puntatore (array) forza Int64 SOLO per il suo slot; gli scalari restano DOUBLE
' (scalarKindMask per-slot). Prima di questo supporto la firma era rifiutata come "mista".
' Uso: ../../tools/sh/parity4 shared/samples/FFI/MixedArrayScalar.bas
DECLARE FUNCTION VecArrScale LIB "libvec_module" ALIAS "vec_array_scale" (buf() AS DOUBLE, n AS LONG, scale AS DOUBLE, offset AS DOUBLE) AS DOUBLE

SUB Main()
    DIM v(2) AS DOUBLE
    v(0) = 1.0 : v(1) = 2.0 : v(2) = 3.0
    PRINT "totale="; VecArrScale(v, 3, 2.5, 0.5)
    PRINT "totale2="; VecArrScale(v, 2, 2.0, 0.5)
END SUB
