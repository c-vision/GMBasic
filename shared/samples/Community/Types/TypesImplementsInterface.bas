' FILE: Types/TypesImplementsInterface.bas
' UDT (TYPE) implementing interfaces: TYPE Foo IMPLEMENTS IBar

IMPORT SYSTEM
IMPORT COLLECTIONS

CLS
PRINT "=== UDT IMPLEMENTS Interface ==="
PRINT

' --- Define interface ---
INTERFACE IShape
    FUNCTION Area() AS DOUBLE
    FUNCTION Describe() AS STRING
END INTERFACE

' --- UDT implementing interface ---
TYPE Rettangolo IMPLEMENTS IShape
    Larghezza AS DOUBLE
    Altezza AS DOUBLE
    FUNCTION Area() AS DOUBLE
        AREA = SELF.Larghezza * SELF.Altezza
    END FUNCTION
    FUNCTION Describe() AS STRING
        DESCRIBE = "Rect(" + STR(SELF.Larghezza) + "x" + STR(SELF.Altezza) + ")"
    END FUNCTION
END TYPE

TYPE Cerchio IMPLEMENTS IShape
    Raggio AS DOUBLE
    FUNCTION Area() AS DOUBLE
        AREA = 3.14159 * SELF.Raggio * SELF.Raggio
    END FUNCTION
    FUNCTION Describe() AS STRING
        DESCRIBE = "Circle(r=" + STR(SELF.Raggio) + ")"
    END FUNCTION
END TYPE

' --- Use via interface variable ---
DIM r AS Rettangolo
r.Larghezza = 10 : r.Altezza = 5
PRINT "Rettangolo:"
PRINT "  Area:    "; r.Area()
PRINT "  Descr:   "; r.Describe()
PRINT

DIM c AS Cerchio
c.Raggio = 7
PRINT "Cerchio:"
PRINT "  Area:    "; c.Area()
PRINT "  Descr:   "; c.Describe()
PRINT

PRINT "=== Fine ==="
END
