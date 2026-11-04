' FILE: TimeSeries/TimeSeriesMatrixGaps.bas
' Copre le sei funzioni TIMESERIES che ritornano matrici e non avevano esempio.
' Chiamate qualificate (TIMESERIES.NOME) così restano rintracciabili dal controllo
' di copertura. Le matrici si leggono con DUE indici: m(i, j).

IMPORT TIMESERIES

SUB Main()
    DIM s(9) AS DOUBLE
    DIM i AS INTEGER
    FOR i = 0 TO 9
        s(i) = 10 + i * 0.5
    NEXT

    DIM s2(9) AS DOUBLE
    FOR i = 0 TO 9
        s2(i) = 5 + i * 0.25
    NEXT

    ' [finestre][2] = [min, max]
    DIM mm = TIMESERIES.TSROLLMINMAX(s, 5)
    PRINT "TSROLLMINMAX finestra0 = min "; mm(0, 0); " max "; mm(0, 1)

    ' 3 x n = [trend, seasonal, residual]
    DIM dec = TIMESERIES.TSDECOMPOSE(s, 2)
    PRINT "TSDECOMPOSE trend[0] = "; dec(0, 0)

    ' m x m covarianza: data = n_obs x n_vars
    DIM data(4) AS ARRAY OF DOUBLE
    data(0) = {1.0, 2.0}
    data(1) = {2.0, 4.0}
    data(2) = {3.0, 6.0}
    data(3) = {4.0, 8.0}
    data(4) = {5.0, 10.0}
    DIM cv = TIMESERIES.TSCOVARIANCEMATRIX(data)
    PRINT "TSCOVARIANCEMATRIX(0,0) = "; cv(0, 0)

    DIM out = TIMESERIES.TSOUTLIERS(s, 2.0)
    PRINT "TSOUTLIERS conteggio = "; UBOUND(out)

    ' [finestre][2] = [slope, intercept]
    DIM rr = TIMESERIES.TSROLLREGRESSION(s, s2, 5)
    PRINT "TSROLLREGRESSION slope0 = "; rr(0, 0)

    DIM rs = TIMESERIES.TSROLLREGRESSIONSINGLE(s, 5)
    PRINT "TSROLLREGRESSIONSINGLE slope0 = "; rs(0, 0)

    PRINT "TimeSeriesMatrixGaps: completato."
END SUB
