' GATE dei NAMESPACE (regola identica al linker C#: `KnownLibraryNotImported`).
' Una funzione di LIBRERIA non si chiama NUDA se il suo namespace non è importato (né è fra
' i default SYSTEM, CONSOLE, GRAPHICS, MATH, STRINGS, DATETIME, CONVERSIONS, IO, CONCURRENCY,
' ARRAYS): qui `ANGLE2D` è di VECTOR e manca `IMPORT VECTOR`, quindi il programma NON deve
' COMPILARE. Le forme lecite sono `IMPORT VECTOR` + `ANGLE2D(...)` oppure `VECTOR.ANGLE2D(...)`
' (coperte da geom/dist2d.bas e math/math_all.bas).
' EXPECT: COMPILE ERROR: namespace non importato
IMPORT STRINGS

SUB Main()
    PRINT ANGLE2D(0, 0, 1, 0)
END SUB
