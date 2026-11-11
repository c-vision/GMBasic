' MiniTest FFI (2026-11-11): UDT PACKED (layout C senza padding, come
' `__attribute__((packed))`): int32@0, double@4, int8@12 → size 13.
' Copre le forme a PUNTATORE (BYREF in/out e array di UDT), dove il buffer è costruito da
' noi e gli offset contano davvero. Il passaggio PER VALORE/RITORNO di un UDT packed è
' rifiutato a compile-time su tutte le gambe (vedi ffi_negative_udt_packed_value.bas):
' la classificazione ABI di libffi non rappresenta un layout senza padding.
' EXPECT: s= 7.5|b.a= 3|b.b= 5|b.c= 2|arr= 16.5
PACKED TYPE PM
    A AS LONG
    B AS DOUBLE
    C AS BYTE
END TYPE

DECLARE FUNCTION PackedSum LIB "liblayout_ext" ALIAS "packed_sum" (BYREF p AS PM) AS DOUBLE
DECLARE SUB PackedBump LIB "liblayout_ext" ALIAS "packed_bump" (BYREF p AS PM)
DECLARE FUNCTION PackedArrSum LIB "liblayout_ext" ALIAS "packed_arr_sum" (p() AS PM, n AS LONG) AS DOUBLE

SUB Main()
    ' int32@0=2, double@4=4.5, int8@12=1 → 7.5 (con il layout NATURALE sarebbe stato 0+0+0)
    DIM p AS PM
    p.A = 2 : p.B = 4.5 : p.C = 1
    PRINT "s="; PackedSum(p)

    ' BYREF in/out: +1 / +0.5 / +1
    PackedBump(p)
    PRINT "b.a="; p.A
    PRINT "b.b="; p.B
    PRINT "b.c="; p.C

    ' Array di UDT packed: 3 elementi contigui da 13 byte
    DIM arr(2) AS PM
    arr(0).A = 1 : arr(0).B = 0.5 : arr(0).C = 1
    arr(1).A = 2 : arr(1).B = 1.5 : arr(1).C = 2
    arr(2).A = 3 : arr(2).B = 2.5 : arr(2).C = 3
    PRINT "arr="; PackedArrSum(arr, 3)
END SUB
