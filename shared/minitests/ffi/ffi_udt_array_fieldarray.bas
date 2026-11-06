' Array di UDT con campi ARRAY (S3 + S3b): copy-in/out e modifica in place, senza toccare
' v(i).A(k) dal BASIC (accesso non supportato su 3 gambe, gap di linguaggio separato).
' EXPECT: s= 16|s2= 28
TYPE Arr4
    A(4) AS LONG
    B AS DOUBLE
END TYPE

DECLARE SUB      Arr4Fill      LIB "liblayout_ext" ALIAS "arr4_fill" (v() AS Arr4, n AS LONG)
DECLARE FUNCTION Arr4BlockSum  LIB "liblayout_ext" ALIAS "arr4_block_sum" (v() AS Arr4, n AS LONG) AS DOUBLE
DECLARE SUB      Arr4BlockBump LIB "liblayout_ext" ALIAS "arr4_block_bump" (v() AS Arr4, n AS LONG)

SUB Main()
    DIM v(1) AS Arr4
    Arr4Fill(v, 2)
    PRINT "s="; Arr4BlockSum(v, 2)

    Arr4BlockBump(v, 2)
    PRINT "s2="; Arr4BlockSum(v, 2)
END SUB
