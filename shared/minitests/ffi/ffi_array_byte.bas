' EXPECT: sum= 15|inc= 2 , 3 , 4 , 5 , 6||sum2= 20||ssum= 0
' Array di BYTE / SBYTE come parametro FFI (2026-11-06, coperto qui dal 2026-11-07).
' Il nativo riceve un `int64_t*`: l'elemento viaggia a 8 byte anche per BYTE/SBYTE
' (scratch + copy-back con saturazione a 0..255 / -128..127). Le estensioni d0..dN-1
' sono emesse automaticamente dopo il puntatore (nessun `n` esplicito nella DECLARE).
DECLARE FUNCTION ByteSum LIB "libbytearr_ext" ALIAS "byte_sum" (a() AS BYTE) AS DOUBLE
DECLARE SUB ByteInc LIB "libbytearr_ext" ALIAS "byte_inc" (a() AS BYTE)
DECLARE FUNCTION SByteSum LIB "libbytearr_ext" ALIAS "sbyte_sum" (a() AS SBYTE) AS DOUBLE

SUB Main()
    DIM i AS LONG
    DIM b(4) AS BYTE
    FOR i = 0 TO 4
        b(i) = i + 1
    NEXT i
    PRINT "sum="; ByteSum(b)

    ByteInc(b)
    PRINT "inc="; b(0); ","; b(1); ","; b(2); ","; b(3); ","; b(4)
    PRINT "|sum2="; ByteSum(b)

    DIM s(2) AS SBYTE
    s(0) = -1 : s(1) = -2 : s(2) = 3
    PRINT "|ssum="; SByteSum(s)
END SUB
