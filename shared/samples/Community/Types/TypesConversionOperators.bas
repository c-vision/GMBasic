' FILE: Types/TypesConversionOperators.bas
' User-defined conversion operators: OPERATOR CTYPE
'
' Sintassi:
'   PUBLIC STATIC OPERATOR CTYPE(source AS T) AS DestType
'       OPERATOR = <converted_value>
'   END OPERATOR

IMPORT SYSTEM
IMPORT COLLECTIONS

CLS
PRINT "=== Type Conversion Operators (CTYPE) ==="
PRINT

' --- Define a class with conversion operators ---
CLASS Punto
    PUBLIC DIM X AS INTEGER
    PUBLIC DIM Y AS INTEGER

    PUBLIC SUB NEW(x AS INTEGER, y AS INTEGER)
        SELF.X = x : SELF.Y = y
    END SUB

    ' Conversion: Punto → STRING
    PUBLIC STATIC OPERATOR CTYPE(p AS Punto) AS STRING
        OPERATOR = "(" + STR(p.X) + "," + STR(p.Y) + ")"
    END OPERATOR

    ' Conversion: Punto → INTEGER (somma coordinate)
    PUBLIC STATIC OPERATOR CTYPE(p AS Punto) AS INTEGER
        OPERATOR = p.X + p.Y
    END OPERATOR
END CLASS

' --- 1. Implicit conversion via CTYPE() ---
PRINT "--- 1. CTYPE() explicit call ---"
DIM p AS Punto = NEW Punto(3, 4)

DIM s$ = CTYPE(p, "STRING")
PRINT "CTYPE(p, STRING):  "; s$

DIM n AS INTEGER = CTYPE(p, "INTEGER")
PRINT "CTYPE(p, INTEGER): "; n
PRINT

' --- 2. Auto-conversion via assignment ---
PRINT "--- 2. Auto via LET ---"
DIM p2 AS Punto = NEW Punto(10, 20)
DIM autoStr$ = p2
DIM autoInt AS INTEGER = p2
PRINT "LET s$ = p2:     "; autoStr$
PRINT "LET n  = p2:     "; autoInt
PRINT

' --- 3. UDT with conversion in expressions ---
PRINT "--- 3. UDT to STRING ---"
TYPE Rettangolo
    Larghezza AS INTEGER
    Altezza AS INTEGER
END TYPE

PUBLIC STATIC OPERATOR CTYPE(r AS Rettangolo) AS STRING
    OPERATOR = STR(r.Larghezza) + "x" + STR(r.Altezza)
END OPERATOR

DIM r AS Rettangolo
r.Larghezza = 5 : r.Altezza = 3
DIM desc$ = r
PRINT "Rettangolo: "; desc$
PRINT

PRINT "=== Fine ==="
END
