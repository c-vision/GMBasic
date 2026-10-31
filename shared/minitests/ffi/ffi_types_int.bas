' EXPECT: int= 1|quad= 1|byte= 1
' Tipi interi FFI: INTEGER/QUAD/BYTE sono tutti marshallati come int64 nativo
' (ABI corretta per `long vec_isprime(long)`). Copre i tipi interi del contratto FFI.
DECLARE FUNCTION i_int LIB "libvec_module" ALIAS "vec_isprime" (n AS INTEGER) AS INTEGER
DECLARE FUNCTION i_quad LIB "libvec_module" ALIAS "vec_isprime" (n AS QUAD) AS QUAD
DECLARE FUNCTION i_byte LIB "libvec_module" ALIAS "vec_isprime" (n AS BYTE) AS BYTE

SUB Main()
    PRINT "int="; i_int(17)
    PRINT "quad="; i_quad(17)
    PRINT "byte="; i_byte(17)
END SUB
