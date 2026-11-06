' Array di UDT (S3, 2026-11-06): il parametro `a() AS Pt2` punta a n struct contigue nel
' layout C, costruite dagli elementi BASIC (copy-in) e ricopiate indietro (copy-out) se il
' nativo le modifica.
' EXPECT: s= 13|s2= 26|x0= 2|y2= 6
TYPE Pt2
    X AS LONG
    Y AS DOUBLE
END TYPE

DECLARE FUNCTION PtsSum LIB "liblayout_ext" ALIAS "pts_sum" (a() AS Pt2) AS DOUBLE
DECLARE SUB PtsScale LIB "liblayout_ext" ALIAS "pts_scale" (a() AS Pt2, k AS DOUBLE)

SUB Main()
    DIM a(2) AS Pt2
    a(0).X = 1 : a(0).Y = 1.5
    a(1).X = 2 : a(1).Y = 2.5
    a(2).X = 3 : a(2).Y = 3.0
    PRINT "s="; PtsSum(a)

    CALL PtsScale(a, 2)
    PRINT "s2="; PtsSum(a)
    PRINT "x0="; a(0).X
    PRINT "y2="; a(2).Y
END SUB
