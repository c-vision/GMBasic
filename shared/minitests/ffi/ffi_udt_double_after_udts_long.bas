' Due UDT da 16 byte (2 DOUBLE, HFA) per valore + tolleranza DOUBLE, ritorno LONG (forma di COMPLEX.EQ).
' EXPECT: tol= 1 |tolno= 0|tolzero= 0
TYPE Cplx16
    Re AS DOUBLE
    Im AS DOUBLE
END TYPE
DECLARE FUNCTION CEqTolL LIB "libudt_ext" ALIAS "cplx16_eq_tol_l" (a AS Cplx16, b AS Cplx16, tol AS DOUBLE) AS LONG

SUB Main()
    DIM a AS Cplx16
    DIM b AS Cplx16
    a.Re = 3 : a.Im = 4
    b.Re = 3.05 : b.Im = 4
    PRINT "tol="; CEqTolL(a, b, 0.1)
    PRINT "tolno="; CEqTolL(a, b, 0.01)
    PRINT "tolzero="; CEqTolL(a, b, 0)
END SUB
