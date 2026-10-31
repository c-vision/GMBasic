' FILE: TimeSeries/TimeSeriesUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: TIMESERIES - FUNZIONI NON ANCORA COPERTE DAGLI ESEMPI
' =================================================================================================
' Questo test copre le funzioni TIMESERIES che non comparivano in alcun esempio .bas:
'   TSROLLAUTOCORR, TSROLLDELTA.
'
' COPERTURA:
' - TIMESERIES.TSROLLAUTOCORR(serie, periodo, lag): autocorrelazione mobile -> ARRAY.
' - TIMESERIES.TSROLLDELTA(serie, periodo)        : delta mobile -> ARRAY.
' =================================================================================================
IMPORT TIMESERIES AS TS

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "TIMESERIES - Funzioni scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    ' Serie sintetica di 24 osservazioni
    DIM S(23) AS DOUBLE
    FOR i = 0 TO 23
        S(i) = SIN(i / 2.0) + 1.0
    NEXT

    ' 1. TSROLLAUTOCORR
    PRINT "-- TSROLLAUTOCORR --"
    DIM ac() AS DOUBLE
    ac() = TS.TSROLLAUTOCORR(S, 10, 1)
    PRINT "TSROLLAUTOCORR(S, 10, 1): "; UBOUND(ac()) + 1; " valori; primo = "; ac(0)
    PRINT

    ' 2. TSROLLDELTA
    PRINT "-- TSROLLDELTA --"
    DIM dl() AS DOUBLE
    dl() = TS.TSROLLDELTA(S, 10)
    PRINT "TSROLLDELTA(S, 10): "; UBOUND(dl()) + 1; " valori; primo = "; dl(0)
    PRINT

    PRINT "Test Completato."
END SUB