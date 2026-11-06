' Campo ARRAY dentro uno UDT (S3b, 2026-11-06): A(4) → 5 elementi contigui nel layout C.
' EXPECT: sz= 32|s= 16|b0= 1|b4= 5|b= 6.5
TYPE Arr4
    A(4) AS LONG
    B AS DOUBLE
END TYPE

DECLARE FUNCTION Arr4Size LIB "liblayout_ext" ALIAS "arr4_size" () AS LONG
DECLARE FUNCTION Arr4Sum LIB "liblayout_ext" ALIAS "arr4_sum" (v AS Arr4) AS DOUBLE
DECLARE FUNCTION Arr4Bump LIB "liblayout_ext" ALIAS "arr4_bump" (v AS Arr4) AS Arr4

SUB Main()
    PRINT "sz="; Arr4Size()
    DIM v AS Arr4
    v.A(0) = 0 : v.A(1) = 1 : v.A(2) = 2 : v.A(3) = 3 : v.A(4) = 4
    v.B = 6
    PRINT "s="; Arr4Sum(v)

    DIM r AS Arr4
    r = Arr4Bump(v)
    PRINT "b0="; r.A(0)
    PRINT "b4="; r.A(4)
    PRINT "b="; r.B
END SUB
