' Due callback nella STESSA dichiarazione e con la STESSA forma (kind, arità): il pool
' dei backend C ha 2 trampolini per forma, quindi la chiamata funziona su tutte e 4 le
' gambe (2026-11-11). Il caso con 3 è il MiniTest negativo ffi_negative_callback_three.
' EXPECT: r= 15|r2= 15
DECLARE FUNCTION CbApply2 LIB "liblayout_ext" ALIAS "cb_apply2" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, g AS FUNCTION(x AS DOUBLE) AS DOUBLE, x AS DOUBLE) AS DOUBLE

SUB Main()
    PRINT "r="; CbApply2(FN(x AS DOUBLE) AS DOUBLE => x * 2, FN(x AS DOUBLE) AS DOUBLE => x * x, 3)
    PRINT "r2="; CbApply2(FN(x AS DOUBLE) AS DOUBLE => x * x, FN(x AS DOUBLE) AS DOUBLE => x * 2, 3)
END SUB
