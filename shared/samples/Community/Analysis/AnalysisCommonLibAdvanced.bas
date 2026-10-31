REM FILE: AnalysisCommonLibAdvanced.bas
REM Copertura delle funzioni applicative di CommonLib.Numerics.Advanced esposte in ANALYSIS.

IMPORT ANALYSIS

SUB Main()
    DIM autocorrelation() = {1.0, 0.7, 0.4, 0.2}
    DIM residuals() = {0.2, -0.1, 0.3, -0.2, 0.1}
    DIM first() = {1.0, 2.0, 3.0}
    DIM second() = {4.0, 6.0, 3.0}

    DIM ar() = ANALYSIS.YULEWALKER(autocorrelation, 2)
    DIM ma() = ANALYSIS.MACOEFFICIENTS(residuals, 2)
    DIM distanceSquared = ANALYSIS.EUCLIDEANDISTANCESQUARED(first, second)

    PRINT "YULEWALKER="; ar(0); ","; ar(1)
    PRINT "MACOEFFICIENTS="; ma(0); ","; ma(1)
    PRINT "EUCLIDEANDISTANCESQUARED="; distanceSquared
END SUB
