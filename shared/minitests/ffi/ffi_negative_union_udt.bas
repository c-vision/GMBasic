' MiniTest NEGATIVO FFI (2026-11-11): un UDT che è una UNION (campi sovrapposti) non è
' rappresentabile nel marshalling FFI (che appiattisce lo UDT in foglie con offset propri):
' le 4 gambe devono rifiutarlo a compile-time con lo stesso messaggio.
' EXPECT: ERROR: FFI 'packed_sum': lo UDT 'Val' usato per il parametro 'p' è una UNION (campi sovrapposti): non ammesso dal marshalling FFI.
UNION Val
    I AS LONG
    F AS DOUBLE
END UNION

DECLARE FUNCTION PackedSum LIB "liblayout_ext" ALIAS "packed_sum" (BYREF p AS Val) AS DOUBLE

SUB Main()
    DIM v AS Val
    v.I = 1
    PRINT PackedSum(v)
END SUB
