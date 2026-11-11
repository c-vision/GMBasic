' MiniTest NEGATIVO FFI: 3 callback con la STESSA forma (kind, arità) nella stessa
' dichiarazione. I backend C hanno 2 trampolini per forma: la DECLARE va rifiutata a
' compile-time con lo stesso messaggio sulle 4 gambe (invece di fallire a runtime su VM/C
' e andare in crash su C99Native, come prima del 2026-11-11).
' EXPECT: ERROR: FFI 'cb_apply3': 3 callback con la stessa forma (kind, arità) nella stessa dichiarazione — i backend C hanno 2 trampolini per forma (massimo 2).
DECLARE FUNCTION CbApply3 LIB "liblayout_ext" ALIAS "cb_apply3" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, g AS FUNCTION(x AS DOUBLE) AS DOUBLE, h AS FUNCTION(x AS DOUBLE) AS DOUBLE, x AS DOUBLE) AS DOUBLE

SUB Main()
    PRINT "r="; CbApply3(FN(x AS DOUBLE) AS DOUBLE => x, FN(x AS DOUBLE) AS DOUBLE => x * 2, FN(x AS DOUBLE) AS DOUBLE => x * x, 3)
END SUB
