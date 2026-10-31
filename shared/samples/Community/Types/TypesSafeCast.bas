' FILE: Types/TypesSafeCast.bas
' Safe cast operator: expr AS? TypeName — returns NIL on failure instead of error

IMPORT SYSTEM
IMPORT COLLECTIONS

CLS
PRINT "=== Safe Cast (AS?) ==="
PRINT

' --- AS? between primitive types ---
PRINT "--- 1. AS? tra primitivi ---"
DIM n AS INTEGER = 42
DIM d AS DOUBLE = n AS? DOUBLE
PRINT "INTEGER AS? DOUBLE:  "; d

DIM s$ = "123"
DIM val AS INTEGER = s$ AS? INTEGER
PRINT "STRING AS? INTEGER:  "; val

DIM bad AS INTEGER = "abc" AS? INTEGER
PRINT "bad STRING AS? INT:  "; bad
PRINT "bad IS NIL? "; (bad IS NIL)
PRINT

' --- AS? with UDT conversion via CTYPE ---
PRINT "--- 2. AS? con CTYPE ---"
CLASS Punto
    PUBLIC DIM X AS INTEGER
    PUBLIC DIM Y AS INTEGER
    PUBLIC SUB NEW(x AS INTEGER, y AS INTEGER)
        SELF.X = x : SELF.Y = y
    END SUB
    PUBLIC STATIC OPERATOR CTYPE(p AS Punto) AS STRING
        OPERATOR = "(" + STR(p.X) + "," + STR(p.Y) + ")"
    END OPERATOR
END CLASS

DIM p AS Punto = NEW Punto(3, 4)
DIM desc = p AS? STRING
PRINT "Punto AS? STRING:  "; desc

' --- AS? between incompatible types returns NIL ---
PRINT "--- 3. AS? incompatibile = NIL ---"
DIM arr() = {1, 2, 3}
DIM asStr = arr AS? STRING
PRINT "array AS? STRING:  "; asStr
PRINT "IS NIL? "; (asStr IS NIL)
PRINT

PRINT "=== Fine ==="
END
