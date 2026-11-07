' Banco FFI (2026-11-07): costo di una chiamata double->double e di un loop locale.
' Misura con ft_now_ms (orologio monotono) N chiamate FFI e N addizioni locali.
' Uso: tools/sh/ffi_bench.sh  (gira le 4 gambe e stampa ns/chiamata).
DECLARE FUNCTION FT_ID2 LIB "ffitest" ALIAS "ft_id2" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
DECLARE FUNCTION FT_NOW_MS LIB "ffitest" ALIAS "ft_now_ms" () AS DOUBLE
DECLARE FUNCTION FT_SUM12 LIB "ffitest" ALIAS "ft_sum12" (a1 AS DOUBLE, a2 AS DOUBLE, a3 AS DOUBLE, a4 AS DOUBLE, a5 AS DOUBLE, a6 AS DOUBLE, a7 AS DOUBLE, a8 AS DOUBLE, a9 AS DOUBLE, a10 AS DOUBLE, a11 AS DOUBLE, a12 AS DOUBLE) AS DOUBLE

SUB Main()
    DIM N AS LONG = 1000000
    DIM i AS LONG
    DIM t0 AS DOUBLE, t1 AS DOUBLE
    DIM x AS DOUBLE, y AS DOUBLE
    x = 0.0 : y = 0.0

    ' warm-up: JIT, delegate, libffi, cache
    FOR i = 1 TO 10000
        x = FT_ID2(x, 1.0)
    NEXT i

    t0 = FT_NOW_MS()
    FOR i = 1 TO N
        x = FT_ID2(x, 1.0)
    NEXT i
    t1 = FT_NOW_MS()
    PRINT "ffi_ns="; CLNG((t1 - t0) * 1000000.0 / N)

    t0 = FT_NOW_MS()
    FOR i = 1 TO N
        y = y + 1.0
    NEXT i
    t1 = FT_NOW_MS()
    PRINT "base_ns="; CLNG((t1 - t0) * 1000000.0 / N)

    ' 12 argomenti: su VM/C e C99 e' il percorso libffi (>8) con la cif in cache.
    DIM z AS DOUBLE
    t0 = FT_NOW_MS()
    FOR i = 1 TO N
        z = FT_SUM12(1,1,1,1,1,1,1,1,1,1,1,1)
    NEXT i
    t1 = FT_NOW_MS()
    PRINT "ffi12_ns="; CLNG((t1 - t0) * 1000000.0 / N)

    PRINT "x="; x; " y="; y; " z="; z
END SUB
