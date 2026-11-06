' Array di UDT (S3, 2026-11-06): il parametro `a() AS Pt2` punta a n struct contigue nel
' layout C, costruite dagli elementi BASIC (copy-in). Lettura verificata su 4 gambe.
' La modifica IN PLACE dell array dal nativo è supportata su IL/PCode; su VM/C l array di
' UDT viaggia per valore e il copy-out non torna al chiamante (limite noto, documentato).
' EXPECT: s= 13|n= 3
TYPE Pt2
    X AS LONG
    Y AS DOUBLE
END TYPE

DECLARE FUNCTION PtsSum LIB "liblayout_ext" ALIAS "pts_sum" (a() AS Pt2, n AS LONG) AS DOUBLE

SUB Main()
    DIM a(2) AS Pt2
    a(0).X = 1 : a(0).Y = 1.5
    a(1).X = 2 : a(1).Y = 2.5
    a(2).X = 3 : a(2).Y = 3.0
    PRINT "s="; PtsSum(a, 3)
    PRINT "n="; 3
END SUB
