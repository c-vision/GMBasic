' EXPECT: sum= 15|sum0= 0
' Array BYREF elemento LONG, array come INPUT (sola lettura) a una FUNCTION con
' ritorno — asse complementare a ffi_array_byref.bas (SUB/DOUBLE/output).
DECLARE FUNCTION VecISum LIB "libvec_module" ALIAS "vec_isum" (buf() AS LONG) AS LONG

SUB Main()
    DIM v(5) AS LONG
    v(0) = 1 : v(1) = 2 : v(2) = 3 : v(3) = 4 : v(4) = 5
    PRINT "sum="; VecISum(v)
    DIM empty(1) AS LONG
    PRINT "sum0="; VecISum(empty)
END SUB
