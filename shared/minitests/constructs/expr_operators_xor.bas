' MiniTest EXPR.XOR — operatore XOR bitwise (parità 4 backend).
' XOR bitwise (cast long). AND/OR short-circuit (canone condiviso). Precedenza: OR < XOR < AND.
' Oracolo: backfill IL (GMB_MINI_WRITE_EXPECT=1).
' EXPECT: a= 5|b= 6|c= 5|d= 0|e= 240|f=-6|g= 0|h= 5|i= 4|j= 2|k= 11|l=-1|m= 11|n= 3
SUB Main()
    PRINT "a="; 6 XOR 3
    PRINT "b="; 12 XOR 10
    PRINT "c="; 0 XOR 5
    PRINT "d="; 5 XOR 5
    PRINT "e="; 255 XOR 15
    PRINT "f="; 5 XOR -1
    PRINT "g="; -1 XOR -1
    DIM x AS LONG
    DIM y AS LONG
    DIM z AS LONG
    x = 6 : y = 3 : z = 1
    PRINT "h="; x XOR y
    PRINT "i="; x XOR y XOR z
    ' Precedenza: AND (piu' alta) prima di XOR; XOR prima di OR.
    PRINT "j="; 6 AND 3 XOR 1
    PRINT "k="; 8 XOR 1 AND 3
    PRINT "l="; 1 OR 2 XOR 3
    PRINT "m="; (8 XOR (1 AND 3))
    PRINT "n="; ((8 XOR 1) AND 3)
END SUB
