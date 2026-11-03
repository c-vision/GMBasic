' EXPECT: outer= 1 inner= 1   7 , 9|deep= 1  sum= 16
' Inoltro di un PARAMETRO ARRAY a un'altra procedura (2026-11-03): su PCode il
' parametro veniva incapsulato in una ByRefBox e il callee vedeva UBOUND=0 e
' elementi vuoti. Copre anche l'inoltro a due livelli e un SUB che somma.
SUB Inner(x() AS DOUBLE)
    PRINT "inner="; UBOUND(x); " "; x(0); ","; x(1)
END SUB
SUB Deep(x() AS DOUBLE)
    DIM i AS LONG, s AS DOUBLE
    FOR i = 0 TO UBOUND(x)
        s = s + x(i)
    NEXT i
    PRINT "deep="; UBOUND(x); " sum="; s
END SUB
SUB Outer(x() AS DOUBLE)
    PRINT "outer="; UBOUND(x);
    CALL Inner(x)
    CALL Deep(x)
END SUB
SUB Main()
    DIM v(1) AS DOUBLE = {7.0, 9.0}
    CALL Outer(v)
END SUB
