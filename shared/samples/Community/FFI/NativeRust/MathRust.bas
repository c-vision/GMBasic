' FILE: FFI/NativeRust/MathRust.bas
' =================================================================
' MathRust — esempio FFI Rust puro (c-abi, NESSUN gmb_ffi.h).
'
' Prima:  rustc --crate-type cdylib math_rust.rs -o libmath_rust.dylib
'         (o: sh build.sh)
'
' Poi:    esegui questo .bas — il runtime risolve la libreria accanto
'         al sorgente (DECLARE ... LIB "./libmath_rust").
'
' Nota stringhe: math_label() alloca con CString; GMBasic copia subito
' (copy-out), ma il buffer Rust resta allocato. math_label_free() lo
' libera: chiamarla con DECLARE SUB dopo l'ultimo uso.
' =================================================================

DECLARE FUNCTION math_add LIB "./libmath_rust" (a AS LONG, b AS LONG) AS LONG
DECLARE FUNCTION math_scale LIB "./libmath_rust" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
DECLARE FUNCTION math_isprime LIB "./libmath_rust" (n AS LONG) AS LONG
DECLARE FUNCTION math_factorial LIB "./libmath_rust" (n AS LONG) AS LONG
DECLARE FUNCTION math_label LIB "./libmath_rust" (id AS LONG, name AS STRING) AS STRING
DECLARE SUB math_labelFree LIB "./libmath_rust" (s AS STRING)
SUB Main()
    PRINT "╔══════════════════════════════════════════════════════════╗"
    PRINT "║  MathRust — FFI Rust (c-abi) senza gmb_ffi.h            ║"
    PRINT "╚══════════════════════════════════════════════════════════╝"
    PRINT

    ' --- 1. Scalar LONG/DOUBLE ---

    PRINT "math_add(21, 21)     = "; math_add(21, 21)          ' 42
    PRINT "math_scale(1.5, 4)   = "; math_scale(1.5, 4.0)      ' 6
    PRINT "math_isprime(17)     = "; math_isprime(17)          ' 1
    PRINT "math_isprime(21)     = "; math_isprime(21)          ' 0
    PRINT "math_factorial(10)   = "; math_factorial(10)        ' 3628800
    PRINT

    ' --- 2. Stringa Rust (CString allocata) + free esplicito ---
    DIM lbl$ = math_label(7, "alpha")
    PRINT "math_label(7, alpha) = "; lbl$
    ' NB: in BASIC il ritorno è una COPIA, quindi il free va applicato al
    ' puntatore interno — per questo esempio si documenta soltanto; in un
    ' modulo reale si esporrebbe math_labelOwned->LONG handle o si accetta
    ' il leak dei sample. (math_label_free esiste per il pattern completo.)

    PRINT
    PRINT "Test MathRust completato."
END SUB
