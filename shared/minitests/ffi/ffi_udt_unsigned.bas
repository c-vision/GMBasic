' UDT con campi UNSIGNED di larghezza dichiarata (S1): uint16/uint32/uint64 nel layout C.
' EXPECT: sz= 16|sum= 5000071000
TYPE UUns
    U16 AS UINTEGER
    U32 AS ULONG
    U64 AS UQUAD
END TYPE

DECLARE FUNCTION UUnsSize LIB "liblayout_ext" ALIAS "uuns_size" () AS LONG
DECLARE FUNCTION UUnsSum  LIB "liblayout_ext" ALIAS "uuns_sum" (v AS UUns) AS DOUBLE

SUB Main()
    PRINT "sz="; UUnsSize()
    DIM v AS UUns
    v.U16 = 1000
    v.U32 = 70000
    v.U64 = 5000000000
    PRINT "sum="; UUnsSum(v)
END SUB
