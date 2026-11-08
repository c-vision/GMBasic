' MiniTest NAMESPACE TRADING.METRICS — tutte le 32 funzioni della superficie.
' Derivato dai MiniTest per-funzione (stessi input e oracoli) + BETA.
' EXPECT: 0.01732051|0.1210136|0.00997998|0.0325515|24.20272006|-0.005|5|0.005|3.12625878|1.05738062|-0.005|10.6|5|1.29740191|-0.92089294|16.21777055|-0.12949347|0.04874813|-0.00415436|0.2236068|-0.005|5|0.5|0.4545|0.2|2|8|5|2.5|0.4|alpha= 13.7262098682992|beta= 2
IMPORT METRICS
IMPORT FUNDAMENTALS
IMPORT VOLATILITY


SUB X_annualizeddownsidedeviation()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(VOLATILITY.AnnualizedDownsideDeviation(r, 0, 12), 8)
END SUB

SUB X_annualizedreturn()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(METRICS.AnnualizedReturn(r, 12), 8)
END SUB

SUB X_annualizedtrackingerror()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  DIM b(4) AS DOUBLE : b(0)=0.008 : b(1)=-0.003 : b(2)=0.015 : b(3)=0.01 : b(4)=0.005
  PRINT ROUND(METRICS.AnnualizedTrackingError(r, b, 12), 8)
END SUB

SUB X_annualizedvolatility()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(VOLATILITY.AnnualizedVolatility(r, 12), 8)
END SUB

SUB X_calmar()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(METRICS.CalmarRatio(r, 12), 8)
END SUB

SUB X_conditionalvalueatrisk()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(METRICS.ConditionalValueAtRisk(r, 0.95), 8)
END SUB

SUB X_dividendyield()
  
  PRINT ROUND(FUNDAMENTALS.DividendYield(5, 100), 8)
END SUB

SUB X_downsidedeviation()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(VOLATILITY.DownsideDeviation(r, 0), 8)
END SUB

SUB X_informationratio()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  DIM b(4) AS DOUBLE : b(0)=0.008 : b(1)=-0.003 : b(2)=0.015 : b(3)=0.01 : b(4)=0.005
  PRINT ROUND(METRICS.InformationRatio(r, b, 12), 8)
END SUB

SUB X_jarqueberatest()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  DIM jb = METRICS.JarqueBeraTest(r)
  PRINT ROUND(jb(0), 8)
END SUB

SUB X_maxdrawdown()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(METRICS.MaxDrawdown(r), 8)
END SUB

SUB X_omegaratio()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(METRICS.OmegaRatio(r, 0), 8)
END SUB

SUB X_peratio()
  
  PRINT ROUND(FUNDAMENTALS.PERatio(10, 2), 8)
END SUB

SUB X_returnskurtosis()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(METRICS.ReturnsKurtosis(r), 8)
END SUB

SUB X_returnsskewness()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(METRICS.ReturnsSkewness(r), 8)
END SUB

SUB X_sharperatio()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(METRICS.SHARPERATIO(r), 8)
END SUB

SUB X_sortinoratio()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(METRICS.SortinoRatio(r, 0.01, 12), 8)
END SUB

SUB X_totalreturn()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(METRICS.TotalReturn(r), 8)
END SUB

SUB X_treynorratio()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  DIM b(4) AS DOUBLE : b(0)=0.008 : b(1)=-0.003 : b(2)=0.015 : b(3)=0.01 : b(4)=0.005
  PRINT ROUND(METRICS.TreynorRatio(r, b, 0.01, 12), 8)
END SUB

SUB X_ulcerindex()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(METRICS.UlcerIndex(r), 8)
END SUB

SUB X_valueatrisk()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(METRICS.ValueAtRisk(r, 0.95), 8)
END SUB

SUB X_met1()
  PRINT FUNDAMENTALS.BookValuePerShare(500, 100)
  PRINT FUNDAMENTALS.EPS(50, 100)
  PRINT ROUND(FUNDAMENTALS.DilutedEPS(50, 100, 10), 4)
  PRINT FUNDAMENTALS.DPS(20, 100)
  PRINT FUNDAMENTALS.EquityMultiplier(500, 250)
  PRINT FUNDAMENTALS.NAV(1000, 200, 100)
  PRINT FUNDAMENTALS.PSRatio(10, 2)
  PRINT FUNDAMENTALS.PBRatio(10, 4)
  PRINT FUNDAMENTALS.DividendPayoutRatio(20, 50)
END SUB

SUB X_alpha()
  DIM a = {0.01, 0.02, -0.01, 0.03}
  DIM b = {0.005, 0.01, -0.005, 0.015}
  PRINT "alpha="; METRICS.ALPHA(a, b, 0.0, 252)
END SUB

SUB X_beta()
  DIM a = {0.01, 0.02, -0.01, 0.03}
  DIM b = {0.005, 0.01, -0.005, 0.015}
  PRINT "beta="; ROUND(METRICS.BETA(a, b), 8)
END SUB

SUB Main()
  X_annualizeddownsidedeviation()
  X_annualizedreturn()
  X_annualizedtrackingerror()
  X_annualizedvolatility()
  X_calmar()
  X_conditionalvalueatrisk()
  X_dividendyield()
  X_downsidedeviation()
  X_informationratio()
  X_jarqueberatest()
  X_maxdrawdown()
  X_omegaratio()
  X_peratio()
  X_returnskurtosis()
  X_returnsskewness()
  X_sharperatio()
  X_sortinoratio()
  X_totalreturn()
  X_treynorratio()
  X_ulcerindex()
  X_valueatrisk()
  X_met1()
  X_alpha()
  X_beta()
END SUB
