' FILE: FFI/NativeGo/MathGo.bas
' =================================================================
' MathGo — esempio FFI Go (cgo) puro (c-abi, NESSUN gmb_ffi.h).
'
' Prima:  go build -buildmode=c-shared -o libmath_go.dylib math_go.go
'         (o: sh build.sh)
'
' Poi:    esegui questo .bas — il runtime risolve la libreria accanto
'         al sorgente (DECLARE ... LIB "./libmath_go").
'
' Nota stringhe: math_label() alloca con C.CString; GMBasic copia subito,
' ma il buffer C resta allocato. math_label_free() (C.free) lo libera.
' =================================================================

DECLARE FUNCTION math_add LIB "./libmath_go" (a AS LONG, b AS LONG) AS LONG
DECLARE FUNCTION math_scale LIB "./libmath_go" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
DECLARE FUNCTION math_isprime LIB "./libmath_go" (n AS LONG) AS LONG
DECLARE FUNCTION math_factorial LIB "./libmath_go" (n AS LONG) AS LONG
DECLARE FUNCTION math_label LIB "./libmath_go" (id AS LONG, name AS STRING) AS STRING
DECLARE SUB math_labelFree LIB "./libmath_go" (s AS STRING)
SUB Main()
    PRINT "╔══════════════════════════════════════════════════════════╗"
    PRINT "║  MathGo — FFI Go (cgo) senza gmb_ffi.h                   ║"
    PRINT "╚══════════════════════════════════════════════════════════╝"
    PRINT

    ' --- 1. Scalar LONG/DOUBLE ---

    PRINT "math_add(21, 21)     = "; math_add(21, 21)          ' 42
    PRINT "math_scale(1.5, 4)   = "; math_scale(1.5, 4.0)      ' 6
    PRINT "math_isprime(17)     = "; math_isprime(17)          ' 1
    PRINT "math_isprime(21)     = "; math_isprime(21)          ' 0
    PRINT "math_factorial(10)   = "; math_factorial(10)        ' 3628800
    PRINT

    ' --- 2. Stringa Go (C.CString allocata) + free esplicito ---
    DIM lbl$ = math_label(7, "alpha")
    PRINT "math_label(7, alpha) = "; lbl$
    ' NB: il ritorno è una COPIA BASIC; il free C si applicherebbe al puntatore
    ' interno — per l'esempio si documenta il pattern, non si chiama il free.

    PRINT
    PRINT "Test MathGo completato."
END SUB
