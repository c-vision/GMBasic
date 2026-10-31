' FILE: FFI/CallCppMath.bas
' Test FFI con libreria C++ compilata (gm_math.dylib)
' Comandi: DECLARE FUNCTION <nome> LIB "./lib/cpp_lib/gm_cpp_math" (<params>) AS <tipo>

IMPORT SYSTEM

' --- Dichiarazioni FFI per la libreria C++ ---
DECLARE FUNCTION Add LIB "./lib/cpp_lib/gm_cpp_math" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
DECLARE FUNCTION Subtract LIB "./lib/cpp_lib/gm_cpp_math" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
DECLARE FUNCTION Multiply LIB "./lib/cpp_lib/gm_cpp_math" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
DECLARE FUNCTION Divide LIB "./lib/cpp_lib/gm_cpp_math" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
DECLARE FUNCTION Power LIB "./lib/cpp_lib/gm_cpp_math" (base AS DOUBLE, exp AS DOUBLE) AS DOUBLE
DECLARE FUNCTION Factorial LIB "./lib/cpp_lib/gm_cpp_math" (n AS LONG) AS LONG
DECLARE FUNCTION IsPrime LIB "./lib/cpp_lib/gm_cpp_math" (n AS LONG) AS LONG
DECLARE FUNCTION CircleArea LIB "./lib/cpp_lib/gm_cpp_math" (radius AS DOUBLE) AS DOUBLE

SUB Main()
    PRINT "╔══════════════════════════════════════════════════════════╗"
    PRINT "║  FFI: C++ Library (gm_math.dylib)                       ║"
    PRINT "╚══════════════════════════════════════════════════════════╝"
    PRINT

    PRINT "Add(10.5, 3.2) ="; Add(10.5, 3.2)
    PRINT "Subtract(10.5, 3.2) ="; Subtract(10.5, 3.2)
    PRINT "Multiply(5.0, 4.0) ="; Multiply(5.0, 4.0)
    PRINT "Divide(10.0, 3.0) ="; Divide(10.0, 3.0)
    PRINT "Power(2.0, 10.0) ="; Power(2.0, 10.0)
    PRINT "Factorial(10) ="; Factorial(10)
    PRINT "IsPrime(17) ="; IsPrime(17); " (1=primo)"
    PRINT "IsPrime(21) ="; IsPrime(21); " (0=non primo)"
    PRINT "CircleArea(5.0) ="; CircleArea(5.0)
    PRINT

    PRINT "=== C++ FFI completato ==="
END SUB
