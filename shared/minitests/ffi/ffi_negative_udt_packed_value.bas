' MiniTest NEGATIVO FFI (2026-11-11): un UDT PACKED passato PER VALORE (o come RITORNO) non è
' rappresentabile nella classificazione ABI (la VM/C usa libffi, che non esprime un layout
' senza padding): le 4 gambe devono rifiutarlo a compile-time con lo stesso messaggio.
' EXPECT: COMPILE ERROR: FFI 'packed_sum_val': lo UDT PACKED 'PM' non è ammesso per il parametro 'p' per VALORE (layout senza padding non rappresentabile nell'ABI: passa BYREF o come array di UDT).
PACKED TYPE PM
    A AS LONG
    B AS DOUBLE
    C AS BYTE
END TYPE

DECLARE FUNCTION PackedSumVal LIB "liblayout_ext" ALIAS "packed_sum_val" (p AS PM) AS DOUBLE

SUB Main()
    DIM p AS PM
    PRINT PackedSumVal(p)
END SUB
