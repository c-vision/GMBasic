' Array di UDT con campi ARRAY (S3 + S3b): popolato dal BASIC, copy-in/out e modifica in place.
' EXPECT: s= 16|s2= 28|a0= 2|a4= 6
TYPE Arr4
    A(4) AS LONG
    B AS DOUBLE
END TYPE

DECLARE FUNCTION Arr4BlockSum  LIB "liblayout_ext" ALIAS "arr4_block_sum" (v() AS Arr4) AS DOUBLE
DECLARE SUB      Arr4BlockBump LIB "liblayout_ext" ALIAS "arr4_block_bump" (v() AS Arr4)

SUB Main()
    DIM v(1) AS Arr4
    v(0).A(0) = 1 : v(0).A(4) = 5 : v(0).B = 1
    v(1).A(0) = 2 : v(1).A(4) = 6 : v(1).B = 1
    PRINT "s="; Arr4BlockSum(v)

    Arr4BlockBump(v)
    PRINT "s2="; Arr4BlockSum(v)
    PRINT "a0="; v(0).A(0)
    PRINT "a4="; v(0).A(4)
END SUB
