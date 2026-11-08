' Argomento UDT che e' il risultato di un'altra chiamata FFI, e DOUBLE dopo due UDT da 16 byte per valore.
' EXPECT: eq= 1|eqv= 1|tol= 1|tolno= 0|tolres= 1
TYPE Cplx16
    Re AS DOUBLE
    Im AS DOUBLE
END TYPE
DECLARE FUNCTION CMake LIB "libudt_ext" ALIAS "cplx16_make" (re AS DOUBLE, im AS DOUBLE) AS Cplx16
DECLARE FUNCTION CEq LIB "libudt_ext" ALIAS "cplx16_eq" (a AS Cplx16, b AS Cplx16) AS DOUBLE
DECLARE FUNCTION CEqTol LIB "libudt_ext" ALIAS "cplx16_eq_tol" (a AS Cplx16, b AS Cplx16, tol AS DOUBLE) AS DOUBLE

SUB Main()
    DIM a AS Cplx16
    DIM b AS Cplx16
    a = CMake(3, 4)
    PRINT "eq="; CEq(a, CMake(3, 4))
    b = CMake(3, 4)
    PRINT "eqv="; CEq(a, b)
    b = CMake(3.05, 4)
    PRINT "tol="; CEqTol(a, b, 0.1)
    PRINT "tolno="; CEqTol(a, b, 0.01)
    PRINT "tolres="; CEqTol(a, CMake(3.05, 4), 0.1)
END SUB
