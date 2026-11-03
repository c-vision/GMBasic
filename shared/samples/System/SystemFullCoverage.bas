' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: System/SystemFullCoverage.bas
' Copertura completa delle funzioni System non ancora coperte.
IMPORT SYSTEM
IMPORT IO

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "System - Copertura Completa"
    PRINT "============================================================"
    PRINT

    DIM v = 42
    PRINT "-- VALTYPE --"
    PRINT "VALTYPE(num): "; SYSTEM.VALTYPE(v)
    PRINT "-- COMMAND --"
    PRINT "COMMAND(1) len: "; LEN(SYSTEM.COMMAND(1))
    PRINT "-- FINPUT --"
    PRINT "FINPUT(0) len: "; LEN(IO.FINPUT(1, 0))
    PRINT

    PRINT "Test Completato."
END SUB
