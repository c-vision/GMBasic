REM FILE: TimeSeriesCommonLibNumerics.bas
REM Espone le funzioni CommonLib.Numerics per serie temporali prima non disponibili in BASIC.

IMPORT TIMESERIES AS TS

SUB Main()
    DIM values() = {1.0, 2.0, 4.0, 3.0, 5.0, 7.0}
    DIM returns() = {-0.02, 0.01, 0.03, -0.01, 0.04, 0.02}
    DIM data(1) AS ARRAY OF DOUBLE
    data(0) = {1.0, 2.0, 3.0, 4.0}
    data(1) = {2.0, 4.0, 5.0, 8.0}

    DIM covariance() AS DOUBLE = TS.TSCOVARIANCEMATRIX(data)
    DIM bounds() = TS.TSMINMAX(values)
    DIM means() = TS.TSROLLMEAN(values, 3)
    DIM skewness() = TS.TSROLLSKEWNESS(values, 3)
    DIM kurtosis() = TS.TSROLLKURTOSIS(values, 4)
    DIM medians() = TS.TSROLLMEDIAN(values, 3)
    DIM quantiles() = TS.TSROLLQUANTILE(values, 3, 0.75)
    DIM sharpes() = TS.TSROLLSHARPE(returns, 3, 0.0)
    DIM sortinos() = TS.TSROLLSORTINO(returns, 3, 0.0, 0.0)
    DIM rollingBounds() AS DOUBLE = TS.TSROLLMINMAX(values, 3)
    DIM regression() AS DOUBLE = TS.TSROLLREGRESSIONSINGLE(values, 3)

    PRINT "COV="; covariance(1)
    PRINT "MINMAX="; bounds(0); ","; bounds(1)
    PRINT "MEAN="; means(0)
    PRINT "SKEWNESS="; skewness(0)
    PRINT "KURTOSIS="; kurtosis(0)
    PRINT "MEDIAN="; medians(0)
    PRINT "QUANTILE="; quantiles(0)
    PRINT "SHARPE="; sharpes(0)
    PRINT "SORTINO="; sortinos(0)
    PRINT "ROLLMINMAX="; rollingBounds(0); ","; rollingBounds(1)
    PRINT "REGRESSION="; regression(0); ","; regression(1)
END SUB
