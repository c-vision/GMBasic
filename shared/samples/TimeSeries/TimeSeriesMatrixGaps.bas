' FILE: TimeSeries/TimeSeriesMatrixGaps.bas
' Copre le sei funzioni TIMESERIES (FFI) che ritornano matrici/vettori strutturati.
' Chiamate qualificate (TIMESERIESX.NOME) così restano rintracciabili dal controllo
' di copertura. Le matrici sono PIATTE: row-major (o coppie interleaved).

IMPORT TIMESERIESX

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

    ' [min0, max0, min1, max1, ...]
    DIM mm() AS DOUBLE = TIMESERIESX.TSROLLMINMAX(s, 5)
    PRINT "TSROLLMINMAX finestra0 = min "; mm(0); " max "; mm(1)

    ' 3*n flat: [trend | seasonal | residual]
    DIM dec() AS DOUBLE = TIMESERIESX.TSDECOMPOSE(s, 2)
    PRINT "TSDECOMPOSE trend[0] = "; dec(0)

    ' m x m covarianza: data = n_obs x n_vars
    DIM data(4) AS ARRAY OF DOUBLE
    data(0) = {1.0, 2.0}
    data(1) = {2.0, 4.0}
    data(2) = {3.0, 6.0}
    data(3) = {4.0, 8.0}
    data(4) = {5.0, 10.0}
    DIM cv() AS DOUBLE = TIMESERIESX.TSCOVARIANCEMATRIX(data)
    PRINT "TSCOVARIANCEMATRIX(0,0) = "; cv(0)

    DIM idx(9) AS LONG
    DIM nout AS LONG = TIMESERIESX.TSOUTLIERS(s, 2.0, idx)
    PRINT "TSOUTLIERS conteggio = "; nout

    ' [slope0, intercept0, slope1, intercept1, ...]
    DIM rr() AS DOUBLE = TIMESERIESX.TSROLLREGRESSION(s, s2, 5)
    PRINT "TSROLLREGRESSION slope0 = "; rr(0)

    DIM rs() AS DOUBLE = TIMESERIESX.TSROLLREGRESSIONSINGLE(s, 5)
    PRINT "TSROLLREGRESSIONSINGLE slope0 = "; rs(0)

    PRINT "TimeSeriesMatrixGaps: completato."
END SUB
