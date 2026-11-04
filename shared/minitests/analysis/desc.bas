SUB Main()
  DIM d(4) AS DOUBLE = {5, 2, 9, 1, 7}
  PRINT STATS.Mean(d)
  PRINT STATS.Median(d)
  PRINT STATS.StdDev(d)
  PRINT STATS.Variance(d)
  PRINT ROUND(STATS.Skewness(d), 12)
  PRINT STATS.Kurtosis(d)
  PRINT STATS.Mad(d)
  DIM e(4) AS DOUBLE = {9, 4, 3, 6, 8}
  PRINT ROUND(STATS.Correlation(d, e), 12)
END SUB
