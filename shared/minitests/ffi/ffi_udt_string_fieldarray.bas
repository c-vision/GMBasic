' UDT con campo STRING (char*) + campo ARRAY: parametro e ritorno (S1 + S3b).
' EXPECT: sz= 24|s= 106|rn= xyz|rv= 3
TYPE NamedVec
    NAME AS STRING
    V(2) AS LONG
END TYPE

DECLARE FUNCTION NvSize LIB "liblayout_ext" ALIAS "nv_size" () AS LONG
DECLARE FUNCTION NvSum  LIB "liblayout_ext" ALIAS "nv_sum" (n AS NamedVec) AS DOUBLE
DECLARE FUNCTION NvMake LIB "liblayout_ext" ALIAS "nv_make" () AS NamedVec

SUB Main()
    PRINT "sz="; NvSize()
    DIM n AS NamedVec
    n.NAME = "xyz" : n.V(0) = 1 : n.V(1) = 2 : n.V(2) = 3
    PRINT "s="; NvSum(n)

    DIM r AS NamedVec
    r = NvMake()
    PRINT "rn="; r.NAME
    PRINT "rv="; r.V(2)
END SUB
