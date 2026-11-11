' MiniTest FFI (2026-11-11): tre callback con la STESSA forma (kind, arità) nella stessa
' dichiarazione. Dal 2026-11-11 non esiste più il limite "2 trampolini per forma": VM/C usa
' closure libffi, C99Native emette un trampolino per PARAMETRO, IL/PCode generano il delegate
' dinamicamente.
' EXPECT: r= 18
DECLARE FUNCTION CbApply3 LIB "liblayout_ext" ALIAS "cb_apply3" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, g AS FUNCTION(x AS DOUBLE) AS DOUBLE, h AS FUNCTION(x AS DOUBLE) AS DOUBLE, x AS DOUBLE) AS DOUBLE

SUB Main()
    PRINT "r="; CbApply3(FN(x AS DOUBLE) AS DOUBLE => x, FN(x AS DOUBLE) AS DOUBLE => x * 2, FN(x AS DOUBLE) AS DOUBLE => x * x, 3)
END SUB
