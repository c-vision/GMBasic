' QUANT (FFI, libgmbquant) — 44 funzioni: general, metrics, volatility, fundamentals.
' Valori esatti verificabili: formule chiuse su serie corte e costanti note.
' EXPECT: zero=20.000000||gordon=28.571429||pref=20.000000||capm=0.114000||prem=0.070000||tsr=0.120000||cgy=0.100000||hpr=0.059870||gmr=0.019571||growth=0.090000||tey=0.080000||spread=0.500000||total=0.034786||ann=0.085530||sharpe=5.982583||sortino=5.556078||calmar=230.185794||info=4.186936||treynor=2.790131||mdd=-0.020000||var=0.000000||cvar=-0.010000||beta=1.650000||alpha=0.526901||skew=-0.475961||kurt=0.589162||jb=0.261097,0.877614||track=0.120374||omega=4.000000||ulcer=1.154701||vol=0.294856||down=0.020000||adown=0.317490||bvps=4.000000||deps=1.000000||payout=0.400000||dy=5.000000||dps=1.000000||eps=2.000000||em=2.000000||nav=3.000000||pb=2.000000||pe=25.000000||ps=5.000000||f=0
IMPORT TRADING_CORE
IMPORT METRICS
IMPORT VOLATILITY
IMPORT FUNDAMENTALS

SUB Main()
  DIM r(4) AS DOUBLE
  DIM b(4) AS DOUBLE
  DIM g(2) AS DOUBLE
  DIM jb() AS DOUBLE

  r(0) = 0.01 : r(1) = -0.02 : r(2) = 0.03 : r(3) = 0.00 : r(4) = 0.015
  b(0) = 0.005 : b(1) = -0.01 : b(2) = 0.02 : b(3) = 0.00 : b(4) = 0.01
  g(0) = 0.05 : g(1) = -0.02 : g(2) = 0.03

  ' ── general ───────────────────────────────────────────────────────────────
  PRINT "zero="; FORMAT(TRADING_CORE.StockValueZeroGrowth(2, 0.10), "#.000000")
  PRINT "|gordon="; FORMAT(TRADING_CORE.StockValueConstGrowth(2, 0.10, 0.03), "#.000000")
  PRINT "|pref="; FORMAT(TRADING_CORE.PreferredStockValue(2, 0.10), "#.000000")
  PRINT "|capm="; FORMAT(TRADING_CORE.CAPM(0.03, 1.2, 0.10), "#.000000")
  PRINT "|prem="; FORMAT(TRADING_CORE.RiskPremium(0.10, 0.03), "#.000000")
  PRINT "|tsr="; FORMAT(TRADING_CORE.TotalStockReturn(100, 110, 2), "#.000000")
  PRINT "|cgy="; FORMAT(TRADING_CORE.CapitalGainsYield(100, 110), "#.000000")
  PRINT "|hpr="; FORMAT(TRADING_CORE.HoldingPeriodReturn(g), "#.000000")
  PRINT "|gmr="; FORMAT(TRADING_CORE.GeometricMeanReturn(g), "#.000000")
  PRINT "|growth="; FORMAT(TRADING_CORE.GrowthRate(0.6, 0.15), "#.000000")
  PRINT "|tey="; FORMAT(TRADING_CORE.TaxEquivalentYield(0.06, 0.25), "#.000000")
  PRINT "|spread="; FORMAT(TRADING_CORE.BidAskSpread(99.5, 100), "#.000000")

  ' ── metrics ───────────────────────────────────────────────────────────────
  PRINT "|total="; FORMAT(METRICS.TotalReturn(r), "#.000000")
  PRINT "|ann="; FORMAT(METRICS.AnnualizedReturn(r, 12), "#.000000")
  PRINT "|sharpe="; FORMAT(METRICS.SHARPERATIO(r), "#.000000")
  PRINT "|sortino="; FORMAT(METRICS.SortinoRatio(r, 0, 252), "#.000000")
  PRINT "|calmar="; FORMAT(METRICS.CalmarRatio(r, 252), "#.000000")
  PRINT "|info="; FORMAT(METRICS.InformationRatio(r, b, 252), "#.000000")
  PRINT "|treynor="; FORMAT(METRICS.TreynorRatio(r, b, 0, 252), "#.000000")
  PRINT "|mdd="; FORMAT(METRICS.MaxDrawdown(r), "#.000000")
  PRINT "|var="; FORMAT(METRICS.ValueAtRisk(r, 0.80), "#.000000")
  PRINT "|cvar="; FORMAT(METRICS.ConditionalValueAtRisk(r, 0.80), "#.000000")
  PRINT "|beta="; FORMAT(METRICS.Beta(r, b), "#.000000")
  PRINT "|alpha="; FORMAT(METRICS.Alpha(r, b, 0, 252), "#.000000")
  PRINT "|skew="; FORMAT(METRICS.ReturnsSkewness(r), "#.000000")
  PRINT "|kurt="; FORMAT(METRICS.ReturnsKurtosis(r), "#.000000")
  jb = METRICS.JarqueBeraTest(r)
  PRINT "|jb="; FORMAT(jb(0), "#.000000"); ","; FORMAT(jb(1), "#.000000")
  PRINT "|track="; FORMAT(METRICS.AnnualizedTrackingError(r, b, 252), "#.000000")
  PRINT "|omega="; FORMAT(METRICS.OmegaRatio(g, 0), "#.000000")
  PRINT "|ulcer="; FORMAT(METRICS.UlcerIndex(g), "#.000000")

  ' ── volatility ────────────────────────────────────────────────────────────
  PRINT "|vol="; FORMAT(VOLATILITY.AnnualizedVolatility(r, 252), "#.000000")
  PRINT "|down="; FORMAT(VOLATILITY.DownsideDeviation(r, 0), "#.000000")
  PRINT "|adown="; FORMAT(VOLATILITY.AnnualizedDownsideDeviation(r, 0, 252), "#.000000")

  ' ── fundamentals ──────────────────────────────────────────────────────────
  PRINT "|bvps="; FORMAT(FUNDAMENTALS.BookValuePerShare(2000, 500), "#.000000")
  PRINT "|deps="; FORMAT(FUNDAMENTALS.DilutedEPS(1000, 500, 500), "#.000000")
  PRINT "|payout="; FORMAT(FUNDAMENTALS.DividendPayoutRatio(200, 500), "#.000000")
  PRINT "|dy="; FORMAT(FUNDAMENTALS.DividendYield(5, 100), "#.000000")
  PRINT "|dps="; FORMAT(FUNDAMENTALS.DPS(500, 500), "#.000000")
  PRINT "|eps="; FORMAT(FUNDAMENTALS.EPS(1000, 500), "#.000000")
  PRINT "|em="; FORMAT(FUNDAMENTALS.EquityMultiplier(2000, 1000), "#.000000")
  PRINT "|nav="; FORMAT(FUNDAMENTALS.NAV(2000, 500, 500), "#.000000")
  PRINT "|pb="; FORMAT(FUNDAMENTALS.PBRatio(50, 25), "#.000000")
  PRINT "|pe="; FORMAT(FUNDAMENTALS.PERatio(50, 2), "#.000000")
  PRINT "|ps="; FORMAT(FUNDAMENTALS.PSRatio(50, 10), "#.000000")
  PRINT "|f=0"
END SUB
