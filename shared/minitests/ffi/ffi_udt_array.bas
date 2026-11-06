' Array di UDT (S3, 2026-11-06): il parametro `a() AS Pt2` punta a n struct contigue nel
' layout C; copy-in dagli elementi BASIC e copy-out dopo la chiamata (la funzione modifica).
' EXPECT: s= 9|s2= 18|x0= 2|y2= 6
TYPE Pt2
    X AS LONG
    Y AS DOUBLE
END TYPE

DECLARE FUNCTION PtsSum LIB "liblayout_ext" ALIAS "pts_sum" (a() AS Pt2, n AS LONG) AS DOUBLE
DECLARE SUB PtsScale LIB "liblayout_ext" ALIAS "pts_scale" (a() AS Pt2, n AS LONG, k AS DOUBLE)

SUB Main()
    DIM a(2) AS Pt2
    a(0).X = 1 : a(0).Y = 1.5
    a(1).X = 2 : a(1).Y = 2.5
    a(2).X = 3 : a(2).Y = 3.0
    PRINT "s="; PtsSum(a, 3)

    CALL PtsScale(a, 3, 2)
    PRINT "s2="; PtsSum(a, 3)
    PRINT "x0="; a(0).X
    PRINT "y2="; a(2).Y
END SUB
