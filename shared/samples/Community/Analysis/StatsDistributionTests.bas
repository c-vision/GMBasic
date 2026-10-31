REM FILE: StatsDistributionTests.bas
REM Normalita, test Z e confronto tra distribuzioni empiriche.

IMPORT STATS

SUB Main()
    DIM sample() = {0.12, -0.31, 0.45, 1.02, -0.76, 0.33, 0.18, -0.22, 0.57, -0.41, 0.08, 0.64, -0.55, 0.29, 0.91, -0.17, 0.38, -0.68, 0.14, 0.49, 0.05, -0.27, 0.73, -0.44, 0.21, 0.36, -0.12, 0.58, -0.35, 0.02}
    DIM reference() = {-0.8, -0.5, -0.3, -0.1, 0.0, 0.2, 0.4, 0.6, 0.8, 1.0}

    DIM shapiro = STATS.SHAPIRO(sample)
    PRINT "Shapiro-Wilk: W="; shapiro.Statistic; ", p="; shapiro.PValue

    DIM normal = STATS.NORMALTEST(sample)
    PRINT "D'Agostino-Pearson: K2="; normal.Statistic; ", p="; normal.PValue

    DIM z = STATS.ZTEST(sample, 0.0, 0.5)
    PRINT "Z test: z="; z.Statistic; ", p="; z.PValue

    DIM ks = STATS.KS(sample, reference)
    PRINT "Kolmogorov-Smirnov: D="; ks.Statistic; ", p="; ks.PValue
END SUB
