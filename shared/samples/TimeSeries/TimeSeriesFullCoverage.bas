' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
IMPORT TYPEINFO
' FILE: TimeSeries/TimeSeriesFullCoverage.bas
' Copertura completa delle funzioni Time Series non ancora coperte.
IMPORT TIMESERIESX AS TS

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "TimeSeries - Copertura Completa"
    PRINT "============================================================"
    PRINT

    DIM s(9) AS DOUBLE
    s(0)=1.0 : s(1)=2.0 : s(2)=3.0 : s(3)=4.0 : s(4)=5.0
    s(5)=4.0 : s(6)=3.0 : s(7)=2.0 : s(8)=1.0 : s(9)=2.0

    ' --- TSROLLMAD ---
    PRINT "-- TSROLLMAD --"
    DIM rm() = TS.TSROLLMAD(s, 3)
    PRINT "TSROLLMAD len: "; LEN(rm())
    PRINT

    ' --- TSROLLAUTOCORR ---
    PRINT "-- TSROLLAUTOCORR --"
    DIM rac() = TS.TSROLLAUTOCORR(s, 3, 1)
    PRINT "TSROLLAUTOCORR len: "; LEN(rac())
    PRINT

    ' --- TSROLLREGRESSION ---
    PRINT "-- TSROLLREGRESSION --"
    DIM rr() = TS.TSROLLREGRESSION(s, s, 3)
    PRINT "TSROLLREGRESSION len: "; LEN(rr())
    PRINT

    ' --- TSDECOMPOSE ---
    PRINT "-- TSDECOMPOSE --"
    DIM dec() AS DOUBLE = TS.TSDECOMPOSE(s, 2)
    PRINT "TSDECOMPOSE type: "; TYPEINFO(dec)
    PRINT

    PRINT "Test Completato."
END SUB
