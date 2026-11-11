' MiniTest FFI (2026-11-11): callback con ARITÀ 17, oltre il limite dei delegate del BCL
' (`System.Func` arriva a 16 argomenti + ritorno, `System.Action` a 16): sulla gamba IL il
' tipo delegate viene generato a runtime (DynamicDelegateTypes), sulle altre il limite è 64.
' EXPECT: a= 8.5
DECLARE FUNCTION CbApply17 LIB "liblayout_ext" ALIAS "cb_apply17" (f AS FUNCTION(p1 AS DOUBLE, p2 AS DOUBLE, p3 AS DOUBLE, p4 AS DOUBLE, p5 AS DOUBLE, p6 AS DOUBLE, p7 AS DOUBLE, p8 AS DOUBLE, p9 AS DOUBLE, p10 AS DOUBLE, p11 AS DOUBLE, p12 AS DOUBLE, p13 AS DOUBLE, p14 AS DOUBLE, p15 AS DOUBLE, p16 AS DOUBLE, p17 AS DOUBLE) AS DOUBLE, x AS DOUBLE) AS DOUBLE

SUB Main()
    ' 17 * 0.5 = 8.5
    PRINT "a="; CbApply17(FN(p1 AS DOUBLE, p2 AS DOUBLE, p3 AS DOUBLE, p4 AS DOUBLE, p5 AS DOUBLE, p6 AS DOUBLE, p7 AS DOUBLE, p8 AS DOUBLE, p9 AS DOUBLE, p10 AS DOUBLE, p11 AS DOUBLE, p12 AS DOUBLE, p13 AS DOUBLE, p14 AS DOUBLE, p15 AS DOUBLE, p16 AS DOUBLE, p17 AS DOUBLE) AS DOUBLE => (p1+p2+p3+p4+p5+p6+p7+p8+p9+p10+p11+p12+p13+p14+p15+p16+p17) * 0.5, 1.0)
END SUB
