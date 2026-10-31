' EXPECT: sum= 15
' Array BYREF elemento QUAD (non solo LONG testato altrove): verifica che il set di
' tipi elemento a 8 byte ammessi (LONG/QUAD/ULONG/UQUAD) funzioni per l'intera
' famiglia, non solo per LONG — stesso meccanismo (int64_t*), larghezza identica.
DECLARE FUNCTION VecISum LIB "libvec_module" ALIAS "vec_isum" (buf() AS QUAD, n AS LONG) AS LONG

SUB Main()
    DIM v(5) AS QUAD
    v(0) = 1 : v(1) = 2 : v(2) = 3 : v(3) = 4 : v(4) = 5
    PRINT "sum="; VecISum(v, 5)
END SUB
