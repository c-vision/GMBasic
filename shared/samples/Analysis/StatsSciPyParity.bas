REM FILE: StatsSciPyParity.bas

IMPORT STATS
IMPORT MATH

SUB Main()
    DIM x() = {2.0, 4.0, 5.0, 7.0, 9.0}
    DIM y() = {1.0, 3.0, 6.0, 8.0, 10.0}
    DIM paired() = {1.5, 3.5, 5.5, 6.5, 8.5}
    DIM observed() = {16.0, 18.0, 16.0, 14.0, 12.0, 12.0}
    DIM expected() = {16.0, 16.0, 16.0, 16.0, 12.0, 12.0}
    DIM normal() = {0.12, -0.31, 0.45, 1.02, -0.76, 0.33, 0.18, -0.22, 0.57, -0.41, 0.08, 0.64, -0.55, 0.29, 0.91, -0.17, 0.38, -0.68, 0.14, 0.49, 0.05, -0.27, 0.73, -0.44, 0.21, 0.36, -0.12, 0.58, -0.35, 0.02}
    DIM groups(1) AS ARRAY OF DOUBLE
    groups(0) = {2.0, 3.0, 4.0, 5.0}
    groups(1) = {4.0, 5.0, 6.0, 7.0}

    DIM result = STATS.TTEST(x, y)
    PRINT "TTEST="; CDBL(MATH.ROUND(result.Statistic, 6)); ","; CDBL(MATH.ROUND(result.PValue, 6))
    result = STATS.TTESTPAIRED(x, paired)
    PRINT "PAIRED="; CDBL(MATH.ROUND(result.Statistic, 6)); ","; CDBL(MATH.ROUND(result.PValue, 6))
    result = STATS.TTESTONESAMPLE(x, 5.0)
    PRINT "ONESAMPLE="; CDBL(MATH.ROUND(result.Statistic, 6)); ","; CDBL(MATH.ROUND(result.PValue, 6))
    result = STATS.ANOVA(groups)
    PRINT "ANOVA="; CDBL(MATH.ROUND(result.Statistic, 6)); ","; CDBL(MATH.ROUND(result.PValue, 6))
    result = STATS.CHI2(observed, expected)
    PRINT "CHI2="; CDBL(MATH.ROUND(result.Statistic, 6)); ","; CDBL(MATH.ROUND(result.PValue, 6))
    result = STATS.FISHER(8, 2, 1, 5)
    PRINT "FISHER="; CDBL(MATH.ROUND(result.Statistic, 6)); ","; CDBL(MATH.ROUND(result.PValue, 6))
    result = STATS.SHAPIRO(normal)
    PRINT "SHAPIRO="; CDBL(MATH.ROUND(result.Statistic, 6)); ","; CDBL(MATH.ROUND(result.PValue, 6))
    result = STATS.NORMALTEST(normal)
    PRINT "NORMALTEST="; CDBL(MATH.ROUND(result.Statistic, 6)); ","; CDBL(MATH.ROUND(result.PValue, 5))
    result = STATS.ZTEST(x, 5.0, 2.0)
    PRINT "ZTEST="; CDBL(MATH.ROUND(result.Statistic, 6)); ","; CDBL(MATH.ROUND(result.PValue, 6))
    result = STATS.KS(x, y)
    PRINT "KS="; CDBL(MATH.ROUND(result.Statistic, 6)); ","; CDBL(MATH.ROUND(result.PValue, 6))
END SUB
