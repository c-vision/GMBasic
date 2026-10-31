' FILE: FFI/CallGoMath.bas
' Test FFI con libreria Go compilata (gm_math.dylib via cgo)
' Comandi: DECLARE FUNCTION <nome> LIB "./lib/go_lib/gm_go_math" (<params>) AS <tipo>

IMPORT SYSTEM

' --- Dichiarazioni FFI per la libreria Go (cgo) ---
DECLARE FUNCTION GoAdd LIB "./lib/go_lib/gm_go_math" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
DECLARE FUNCTION GoSubtract LIB "./lib/go_lib/gm_go_math" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
DECLARE FUNCTION GoMultiply LIB "./lib/go_lib/gm_go_math" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
DECLARE FUNCTION GoDivide LIB "./lib/go_lib/gm_go_math" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
DECLARE FUNCTION GoFactorial LIB "./lib/go_lib/gm_go_math" (n AS LONG) AS LONG
DECLARE FUNCTION GoIsPrime LIB "./lib/go_lib/gm_go_math" (n AS LONG) AS LONG
DECLARE FUNCTION GoCircleArea LIB "./lib/go_lib/gm_go_math" (radius AS DOUBLE) AS DOUBLE

SUB Main()
    PRINT "╔══════════════════════════════════════════════════════════╗"
    PRINT "║  FFI: Go Library (gm_math.dylib via cgo)                ║"
    PRINT "╚══════════════════════════════════════════════════════════╝"
    PRINT

    PRINT "GoAdd(10.5, 3.2) ="; GoAdd(10.5, 3.2)
    PRINT "GoSubtract(10.5, 3.2) ="; GoSubtract(10.5, 3.2)
    PRINT "GoMultiply(5.0, 4.0) ="; GoMultiply(5.0, 4.0)
    PRINT "GoDivide(10.0, 3.0) ="; GoDivide(10.0, 3.0)
    PRINT "GoFactorial(10) ="; GoFactorial(10)
    PRINT "GoIsPrime(17) ="; GoIsPrime(17); " (1=primo)"
    PRINT "GoIsPrime(21) ="; GoIsPrime(21); " (0=non primo)"
    PRINT "GoCircleArea(5.0) ="; GoCircleArea(5.0)
    PRINT

    PRINT "=== Go FFI completato ==="
END SUB
