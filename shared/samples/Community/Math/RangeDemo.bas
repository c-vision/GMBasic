' FILE: Math/RangeDemo.bas
' =================================================================================================
' MATH.RANGE — genera una sequenza numerica con passo opzionale.
' =================================================================================================
' RANGE(start, end)        → start..end con passo 1
' RANGE(start, end, step)  → passo step (negativo per sequenze decrescenti)
' Si usa in FOR EACH (iterazione) e con LEN/UBOUND se assegnata a una variabile.
' =================================================================================================
IMPORT MATH

SUB Main()
    CLS
    PRINT "MATH.RANGE — Sequenze numeriche"
    PRINT "==============================="
    PRINT

    PRINT "RANGE(1, 5)     = ";
    FOR EACH n IN RANGE(1, 5)
        PRINT n; " ";
    NEXT
    PRINT

    PRINT "RANGE(0, 10, 2) = ";
    FOR EACH n IN RANGE(0, 10, 2)
        PRINT n; " ";
    NEXT
    PRINT

    PRINT "RANGE(10, 0, -5) = ";
    FOR EACH n IN RANGE(10, 0, -5)
        PRINT n; " ";
    NEXT
    PRINT

    ' Somma dei valori della sequenza
    DIM somma = 0
    FOR EACH n IN RANGE(1, 100)
        somma = somma + n
    NEXT
    PRINT "Somma RANGE(1, 100) = "; somma
END SUB
