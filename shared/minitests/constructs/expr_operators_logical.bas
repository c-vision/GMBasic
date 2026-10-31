' MiniTest EXPR.LOGICAL — AND/OR short-circuit (parità 4 backend).
' Semantica canonica (canone con Compiler/IL/PCode/VM-C/C99Native):
'   AND: se sinistra FALSA -> 0, altrimenti VALORE destro.
'   OR:  se sinistra VERA  -> -1, altrimenti VALORE destro.
' Oracolo: backfill IL (GMB_MINI_WRITE_EXPECT=1).
' EXPECT: and63= 3|or10=-1|or05= 5|and05= 0|and70= 0|andz= 0|orz=-1|sc_and= 0|sc_or=-1
SUB Main()
    PRINT "and63="; 6 AND 3
    PRINT "or10="; 1 OR 0
    PRINT "or05="; 0 OR 5
    PRINT "and05="; 0 AND 5
    PRINT "and70="; 7 AND 0
    DIM z AS LONG
    z = 0
    PRINT "andz="; 6 AND z
    PRINT "orz="; 6 OR z
    ' short-circuit: il lato destro non deve essere valutato se il sinistro decide
    PRINT "sc_and="; 0 AND Fx()
    PRINT "sc_or="; 1 OR Fx()
END SUB

FUNCTION Fx() AS DOUBLE
    PRINT "SIDE"
    Fx = 99
END FUNCTION
