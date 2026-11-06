' Campo ARRAY di un UDT su istanze GLOBALI, GLOBAL-in-SUB e NEW (S3b, 2026-11-06).
' EXPECT: gsum= 7|g5sum= 9|n1= 3|nsum= 3|r0= 2|r4= 6|rb= 1.5
TYPE Arr4
    A(4) AS LONG
    B AS DOUBLE
END TYPE

DECLARE FUNCTION Arr4Sum  LIB "liblayout_ext" ALIAS "arr4_sum" (v AS Arr4) AS DOUBLE
DECLARE FUNCTION Arr4Bump LIB "liblayout_ext" ALIAS "arr4_bump" (v AS Arr4) AS Arr4

DIM g4 AS Arr4

SUB FillGlobal5()
    GLOBAL g5 AS Arr4
    g5.A(0) = 2 : g5.A(4) = 6 : g5.B = 1
END SUB

SUB Main()
    g4.A(0) = 1 : g4.A(4) = 5 : g4.B = 1
    PRINT "gsum="; Arr4Sum(g4)

    FillGlobal5()
    PRINT "g5sum="; Arr4Sum(g5)

    DIM n AS Arr4
    n = NEW Arr4
    n.A(1) = 3
    PRINT "n1="; n.A(1)
    PRINT "nsum="; Arr4Sum(n)

    DIM r AS Arr4
    r = Arr4Bump(g4)
    PRINT "r0="; r.A(0)
    PRINT "r4="; r.A(4)
    PRINT "rb="; r.B
END SUB
