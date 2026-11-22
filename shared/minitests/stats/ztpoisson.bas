' MiniTest STATS — ZTPoisson (Poisson troncato a zero) con lambda=1.
' CONVENZIONE BASIC dei pmf/cdf discreti: (x, parametro) — STATS.ZTPoissonPmf(k, lambda).
' P(X=1) = e^-1/(1-e^-1) = 0.5819767; P(X=2) = P(X=1)/2; CDF(2) = 0.8729651.
' Copre la migrazione di ZTPoissonPmf/Cdf al kernel C (gmb_dist_ztpoisson_*).
' EXPECT: ztpmf= 0.581977|ztcdf= 0.872965
IMPORT STATS
SUB Main()
  PRINT "ztpmf="; MATH.ROUND(STATS.ZTPoissonPmf(1, 1), 6)
  PRINT "ztcdf="; MATH.ROUND(STATS.ZTPoissonCdf(2, 1), 6)
END SUB
