' MiniTest NAMESPACE TRADING.METRICS — tutte le 32 funzioni della superficie.
' Derivato dai MiniTest per-funzione (stessi input e oracoli) + BETA.
' EXPECT: 0.01732051|0.1210136|0.00997998|0.0325515|24.20272006|-0.005|5|0.005|3.12625878|1.05738062|-0.005|10.6|5|1.29740191|-0.92089294|16.21777055|-0.12949347|0.04874813|-0.00415436|0.2236068|-0.005|5|0.5|0.4545|0.2|2|8|5|2.5|0.4|alpha= 13.7262098682992|beta= 2
IMPORT TRADING.METRICS


SUB X_annualizeddownsidedeviation()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(TRADING.METRICS.AnnualizedDownsideDeviation(r, 0, 12), 8)
END SUB

SUB X_annualizedreturn()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(TRADING.METRICS.AnnualizedReturn(r, 12), 8)
END SUB

SUB X_annualizedtrackingerror()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  DIM b(4) AS DOUBLE : b(0)=0.008 : b(1)=-0.003 : b(2)=0.015 : b(3)=0.01 : b(4)=0.005
  PRINT ROUND(TRADING.METRICS.AnnualizedTrackingError(r, b, 12), 8)
END SUB

SUB X_annualizedvolatility()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(TRADING.METRICS.AnnualizedVolatility(r, 12), 8)
END SUB

SUB X_calmar()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(TRADING.METRICS.CalmarRatio(r, 12), 8)
END SUB

SUB X_conditionalvalueatrisk()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(TRADING.METRICS.ConditionalValueAtRisk(r, 0.95), 8)
END SUB

SUB X_dividendyield()
  
  PRINT ROUND(TRADING.METRICS.DividendYield(5, 100), 8)
END SUB

SUB X_downsidedeviation()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(TRADING.METRICS.DownsideDeviation(r, 0), 8)
END SUB

SUB X_informationratio()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  DIM b(4) AS DOUBLE : b(0)=0.008 : b(1)=-0.003 : b(2)=0.015 : b(3)=0.01 : b(4)=0.005
  PRINT ROUND(TRADING.METRICS.InformationRatio(r, b, 12), 8)
END SUB

SUB X_jarqueberatest()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  DIM jb = TRADING.METRICS.JarqueBeraTest(r)
  PRINT ROUND(jb(0), 8)
END SUB

SUB X_maxdrawdown()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(TRADING.METRICS.MaxDrawdown(r), 8)
END SUB

SUB X_omegaratio()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(TRADING.METRICS.OmegaRatio(r, 0), 8)
END SUB

SUB X_peratio()
  
  PRINT ROUND(TRADING.METRICS.PERatio(10, 2), 8)
END SUB

SUB X_returnskurtosis()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(TRADING.METRICS.ReturnsKurtosis(r), 8)
END SUB

SUB X_returnsskewness()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(TRADING.METRICS.ReturnsSkewness(r), 8)
END SUB

SUB X_sharperatio()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(TRADING.METRICS.SHARPERATIO(r), 8)
END SUB

SUB X_sortinoratio()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(TRADING.METRICS.SortinoRatio(r, 0.01, 12), 8)
END SUB

SUB X_totalreturn()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(TRADING.METRICS.TotalReturn(r), 8)
END SUB

SUB X_treynorratio()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  DIM b(4) AS DOUBLE : b(0)=0.008 : b(1)=-0.003 : b(2)=0.015 : b(3)=0.01 : b(4)=0.005
  PRINT ROUND(TRADING.METRICS.TreynorRatio(r, b, 0.01, 12), 8)
END SUB

SUB X_ulcerindex()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(TRADING.METRICS.UlcerIndex(r), 8)
END SUB

SUB X_valueatrisk()
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(TRADING.METRICS.ValueAtRisk(r, 0.95), 8)
END SUB

SUB X_met1()
  PRINT TRADING.METRICS.BookValuePerShare(500, 100)
  PRINT TRADING.METRICS.EPS(50, 100)
  PRINT ROUND(TRADING.METRICS.DilutedEPS(50, 100, 10), 4)
  PRINT TRADING.METRICS.DPS(20, 100)
  PRINT TRADING.METRICS.EquityMultiplier(500, 250)
  PRINT TRADING.METRICS.NAV(1000, 200, 100)
  PRINT TRADING.METRICS.PSRatio(10, 2)
  PRINT TRADING.METRICS.PBRatio(10, 4)
  PRINT TRADING.METRICS.DividendPayoutRatio(20, 50)
END SUB

SUB X_alpha()
  DIM a = {0.01, 0.02, -0.01, 0.03}
  DIM b = {0.005, 0.01, -0.005, 0.015}
  PRINT "alpha="; TRADING.METRICS.ALPHA(a, b, 0.0, 252)
END SUB

SUB X_beta()
  DIM a = {0.01, 0.02, -0.01, 0.03}
  DIM b = {0.005, 0.01, -0.005, 0.015}
  PRINT "beta="; ROUND(TRADING.METRICS.BETA(a, b), 8)
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
