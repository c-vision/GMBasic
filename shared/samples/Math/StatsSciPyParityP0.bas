' scipy.stats P0: tutti i 20 test d'ipotesi STATS, sample completo e deterministico.
' Oracoli: scipy.stats 1.18. Risultati in `r` riusato (DIM una volta: il C99Native
' tipizza il boxed solo da DIM); input riusati per restare compatto.
IMPORT STATS
IMPORT MATH

SUB Main()
  DIM x(4) AS DOUBLE = {2, 4, 5, 7, 9}
  DIM y(4) AS DOUBLE = {1, 3, 6, 8, 10}
  DIM paired(4) AS DOUBLE = {1.5, 3.5, 5.5, 6.5, 8.5}
  DIM ob(5) AS DOUBLE = {16, 18, 16, 14, 12, 12}
  DIM ex(5) AS DOUBLE = {16, 16, 16, 16, 12, 12}
  DIM z(19) AS DOUBLE = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20}
  DIM normal(9) AS DOUBLE = {0.12, -0.31, 0.45, 1.02, -0.76, 0.33, 0.18, -0.22, 0.57, -0.41}
  DIM v(4) AS DOUBLE = {1, 2, 3, 4, 5}
  DIM an(1) AS ARRAY OF DOUBLE
  an(0) = {2, 3, 4, 5} : an(1) = {4, 5, 6, 7}
  DIM tb(1) AS ARRAY OF DOUBLE
  tb(0) = {10, 20} : tb(1) = {30, 40}

  PRINT "iqr      ="; MATH.ROUND(STATS.IQR(x), 6)

  DIM r = STATS.TTEST(x, y)
  PRINT "ttest    ="; MATH.ROUND(r.Statistic, 6); MATH.ROUND(r.PValue, 6)
  r = STATS.TTESTPAIRED(x, paired)
  PRINT "ttpaired ="; MATH.ROUND(r.Statistic, 6); MATH.ROUND(r.PValue, 6)
  r = STATS.TTESTONESAMPLE(x, 5.0)
  PRINT "ttone    ="; MATH.ROUND(r.Statistic, 6); MATH.ROUND(r.PValue, 6)
  r = STATS.ANOVA(an)
  PRINT "anova    ="; MATH.ROUND(r.Statistic, 6); MATH.ROUND(r.PValue, 6); r.DegreesOfFreedom
  r = STATS.CHI2(ob, ex)
  PRINT "chi2     ="; MATH.ROUND(r.Statistic, 6); MATH.ROUND(r.PValue, 6)
  r = STATS.CHI2CON(tb)
  PRINT "chi2con  ="; MATH.ROUND(r.Statistic, 6); MATH.ROUND(r.PValue, 6); r.DegreesOfFreedom
  r = STATS.FISHER(8, 2, 1, 5)
  PRINT "fisher   ="; MATH.ROUND(r.Statistic, 6); MATH.ROUND(r.PValue, 6)
  r = STATS.ZTEST(x, 5.0, 2.0)
  PRINT "ztest    ="; MATH.ROUND(r.Statistic, 6); MATH.ROUND(r.PValue, 6)
  r = STATS.KS(x, y)
  PRINT "ks       ="; MATH.ROUND(r.Statistic, 6); MATH.ROUND(r.PValue, 6)
  r = STATS.SHAPIRO(normal)
  PRINT "shapiro  ="; MATH.ROUND(r.Statistic, 6); MATH.ROUND(r.PValue, 6)
  r = STATS.NORMALTEST(z)
  PRINT "normtest ="; MATH.ROUND(r.Statistic, 6); MATH.ROUND(r.PValue, 6)
  r = STATS.PEARSONR(x, y)
  PRINT "pearsonr ="; MATH.ROUND(r.Statistic, 6); MATH.ROUND(r.PValue, 6)
  r = STATS.SPEARMANR(x, y)
  PRINT "spearman ="; MATH.ROUND(r.Statistic, 6); MATH.ROUND(r.PValue, 6)
  r = STATS.LINREGRESS(x, y)
  PRINT "linreg   ="; MATH.ROUND(r.Slope, 6); MATH.ROUND(r.Intercept, 6); MATH.ROUND(r.R, 6)
  r = STATS.MANNWHITNEY(x, y)
  PRINT "mannwhit ="; MATH.ROUND(r.Statistic, 6); MATH.ROUND(r.PValue, 6)
  r = STATS.WILCOXON(x, y)
  PRINT "wilcoxon ="; MATH.ROUND(r.Statistic, 6); MATH.ROUND(r.PValue, 6)
  r = STATS.SKEWTEST(z)
  PRINT "skewtest ="; MATH.ROUND(r.Statistic, 6); MATH.ROUND(r.PValue, 6)
  r = STATS.KURTOSISTEST(z)
  PRINT "kurttest ="; MATH.ROUND(r.Statistic, 6); MATH.ROUND(r.PValue, 6)
  r = STATS.JARQUEBERA(z)
  PRINT "jarque   ="; MATH.ROUND(r.Statistic, 6); MATH.ROUND(r.PValue, 6)
  r = STATS.DESCRIBE(v)
  PRINT "describe ="; r.Nobs; MATH.ROUND(r.Mean, 6); MATH.ROUND(r.Variance, 6); MATH.ROUND(r.Skewness, 6); MATH.ROUND(r.Kurtosis, 6)
END SUB
