' EXPECT: apply= 42|combine2= 43|sum_transformed= 30|apply_i2d= 7
' Callback come parametro FFI (2026-09-29, v1: arita' 0-4, firma interna omogenea per kind,
' BYVAL) — vedi AiWiki Bugs/(open) - FFI-Estensioni-Possibili-BYREF-Struct-Callback.md §3bis.
' Copre: callback a 1 argomento (VecCbApply), a 2 argomenti (VecCbCombine2), un callback
' COMBINATO con un parametro array nella stessa firma (VecCbSumTransformed), e un callback il
' cui kind INTERNO (DOUBLE) e' diverso dal kind ESTERNO forzato della dichiarazione (LONG,
' VecCbApplyI2D — un parametro callback forza SEMPRE Int64 sulla firma esterna, indipendente
' dal kind interno del callback stesso).
DECLARE FUNCTION VecCbApply LIB "libvec_module" ALIAS "vec_cb_apply" (cb AS FUNCTION(x AS LONG) AS LONG, x AS LONG) AS LONG
DECLARE FUNCTION VecCbCombine2 LIB "libvec_module" ALIAS "vec_cb_combine2" (cb AS FUNCTION(a AS LONG, b AS LONG) AS LONG, a AS LONG, b AS LONG) AS LONG
DECLARE FUNCTION VecCbSumTransformed LIB "libvec_module" ALIAS "vec_cb_sum_transformed" (buf() AS LONG, cb AS FUNCTION(x AS LONG) AS LONG) AS LONG
DECLARE FUNCTION VecCbApplyI2D LIB "libvec_module" ALIAS "vec_cb_apply_i2d" (cb AS FUNCTION(x AS DOUBLE) AS DOUBLE, unused AS LONG) AS LONG
SUB Main()
    DIM r1 AS LONG = VecCbApply(FN(x AS LONG) AS LONG => x * 2, 21)
    PRINT "apply="; r1
    DIM r2 AS LONG = VecCbCombine2(FN(a AS LONG, b AS LONG) AS LONG => a + b * 10, 3, 4)
    PRINT "combine2="; r2
    DIM buf(3) AS LONG
    buf(0) = 1 : buf(1) = 2 : buf(2) = 3 : buf(3) = 4
    DIM r3 AS LONG = VecCbSumTransformed(buf, FN(x AS LONG) AS LONG => x * x)
    PRINT "sum_transformed="; r3
    DIM r4 AS LONG = VecCbApplyI2D(FN(x AS DOUBLE) AS DOUBLE => x * 2.0, 0)
    PRINT "apply_i2d="; r4
END SUB
