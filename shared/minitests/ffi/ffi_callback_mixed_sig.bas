' MiniTest FFI (2026-11-11): callback con firma interna MISTA `int64_t (*)(int64_t, double)`
' — il trampolino nativo ha la firma PER-SLOT dichiarata in BASIC (prima era ammessa solo una
' firma interna omogenea, tutti LONG oppure tutti DOUBLE) e il kind del ritorno è indipendente.
' EXPECT: m= 8|q= 12
DECLARE FUNCTION CbMixed LIB "liblayout_ext" ALIAS "cb_mixed" (f AS FUNCTION(a AS LONG, b AS DOUBLE) AS DOUBLE, n AS LONG, d AS DOUBLE) AS DOUBLE
DECLARE FUNCTION CbMixedQ LIB "liblayout_ext" ALIAS "cb_mixed_q" (f AS FUNCTION(a AS DOUBLE, b AS LONG, c AS DOUBLE) AS DOUBLE, n AS LONG, d AS DOUBLE) AS DOUBLE

SUB Main()
    ' 5 + 1.5*2 = 8
    PRINT "m="; CbMixed(FN(a AS LONG, b AS DOUBLE) AS DOUBLE => a + b * 2, 5, 1.5)
    ' 1.0 + 5*2 + 1.0 = 12 — mixed, ordine DOUBLE/LONG/DOUBLE
    PRINT "q="; CbMixedQ(FN(a AS DOUBLE, b AS LONG, c AS DOUBLE) AS DOUBLE => a + b * 2 + c, 5, 1.0)
END SUB
