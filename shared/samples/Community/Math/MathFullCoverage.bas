' FILE: Math/MathFullCoverage.bas
' Copertura completa delle funzioni Math non ancora coperte.
IMPORT MATH

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "Math - Copertura Completa"
    PRINT "============================================================"
    PRINT

    DIM m(1,1)
    m(0,0)=4 : m(0,1)=1
    m(1,0)=1 : m(1,1)=3

    DIM v(2) AS DOUBLE
    v(0)=10 : v(1)=20 : v(2)=30
    PRINT "-- AVG --"
    PRINT "AVG = "; MATH.AVG(v)
    PRINT

    PRINT "Test Completato."
END SUB
