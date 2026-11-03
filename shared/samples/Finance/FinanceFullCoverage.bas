' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: Finance/FinanceFullCoverage.bas
' Copertura completa delle funzioni Finance non ancora coperte.
IMPORT FINANCE

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "Finance - Copertura Completa"
    PRINT "============================================================"
    PRINT

    PRINT "-- PV --"
    PRINT "PV(0.05, 10, 100) = "; FINANCE.PV(0.05, 10, 100)
    PRINT "-- NPER --"
    PRINT "NPER(0.05, -100, 1000) = "; FINANCE.NPER(0.05, -100, 1000)
    PRINT

    PRINT "Test Completato."
END SUB
