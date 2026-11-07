' FILE: Quant/QuantLab.bas
' SCENARIO DIDATTICO — analisi quantitativa di un titolo e di un fondo:
'
'   A) valutazione azionaria (TRADING_CORE): dividend discount model (crescita zero
'      e Gordon), azione privilegiata, CAPM, premio al rischio, rendimento totale,
'      capital gain, holding period return, media geometrica, crescita sostenibile,
'      rendimento equivalente tassabile, spread bid-ask.
'   B) performance e rischio del fondo (METRICS): rendimento totale e annualizzato,
'      Sharpe, Sortino, Calmar, Information/Treynor, max drawdown, VaR/CVaR,
'      beta/alpha, skew/kurt, Jarque-Bera, tracking error, Omega, Ulcer.
'   C) volatilità (VOLATILITY): volatilità annualizzata, downside deviation e sua
'      versione annualizzata.
'   D) fondamentali (FUNDAMENTALS): EPS/DPS/NAV, P/E, P/B, P/S, payout, dividend yield.
'
' I numeri sono scelti per avere valori leggibili e verosimili: titolo a 50, dividendo
' 2, crescita 3%, risk-free 3%, mercato 10%; fondo con 5 rendimenti periodici contro
' un benchmark.
'
' Domini: TRADING_CORE, METRICS, VOLATILITY, FUNDAMENTALS. Output deterministico.
IMPORT TRADING_CORE
IMPORT METRICS
IMPORT VOLATILITY
IMPORT FUNDAMENTALS

SUB Main()
  DIM r(4) AS DOUBLE          ' rendimenti del fondo
  DIM b(4) AS DOUBLE          ' rendimenti del benchmark
  DIM g(2) AS DOUBLE          ' rendimenti per holding period / geometrica
  DIM jb() AS DOUBLE

  r(0) = 0.012 : r(1) = -0.018 : r(2) = 0.025 : r(3) = 0.004 : r(4) = 0.011
  b(0) = 0.006 : b(1) = -0.010 : b(2) = 0.018 : b(3) = 0.002 : b(4) = 0.008
  g(0) = 0.05 : g(1) = -0.02 : g(2) = 0.03

  ' ══════════════════════════════════════════════════════════════════════════
  ' A) VALUTAZIONE AZIONARIA
  '    titolo: prezzo 50, dividendo 2, required return 10%, crescita 3%
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "A) valutazione azionaria"
  PRINT " DDM crescita zero (2/0.10)      = "; FORMAT(TRADING_CORE.StockValueZeroGrowth(2, 0.10), "#.000000")
  PRINT " DDM Gordon (2/(0.10-0.03))      = "; FORMAT(TRADING_CORE.StockValueConstGrowth(2, 0.10, 0.03), "#.000000")
  PRINT " privilegiata (2/0.10)           = "; FORMAT(TRADING_CORE.PreferredStockValue(2, 0.10), "#.000000")
  PRINT " CAPM (3% + 1.2*(10%-3%))        = "; FORMAT(TRADING_CORE.CAPM(0.03, 1.2, 0.10), "#.000000")
  PRINT " premio al rischio (10%-3%)      = "; FORMAT(TRADING_CORE.RiskPremium(0.10, 0.03), "#.000000")
  PRINT " rendimento totale (100->110+2)  = "; FORMAT(TRADING_CORE.TotalStockReturn(100, 110, 2), "#.000000")
  PRINT " capital gain (100->110)         = "; FORMAT(TRADING_CORE.CapitalGainsYield(100, 110), "#.000000")
  PRINT " holding period (5,-2,3%)        = "; FORMAT(TRADING_CORE.HoldingPeriodReturn(g), "#.000000")
  PRINT " media geometrica                = "; FORMAT(TRADING_CORE.GeometricMeanReturn(g), "#.000000")
  PRINT " crescita sostenibile (0.6*15%)  = "; FORMAT(TRADING_CORE.GrowthRate(0.6, 0.15), "#.000000")
  PRINT " rend. equiv. tassabile (6%,25%) = "; FORMAT(TRADING_CORE.TaxEquivalentYield(0.06, 0.25), "#.000000")
  PRINT " spread bid-ask (99.5/100)       = "; FORMAT(TRADING_CORE.BidAskSpread(99.5, 100), "#.000000")

  ' ══════════════════════════════════════════════════════════════════════════
  ' B) PERFORMANCE E RISCHIO DEL FONDO
  '    rendimenti periodici in frazione; benchmark per beta/alpha/tracking
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "B) performance e rischio (5 periodi, 252/anno)"
  PRINT " rendimento totale               = "; FORMAT(METRICS.TotalReturn(r), "#.000000")
  PRINT " rendimento annualizzato         = "; FORMAT(METRICS.AnnualizedReturn(r, 12), "#.000000")
  PRINT " Sharpe (1-arg, sqrt 252)        = "; FORMAT(METRICS.SHARPERATIO(r), "#.000000")
  PRINT " Sortino (rf 0, 252)             = "; FORMAT(METRICS.SortinoRatio(r, 0, 252), "#.000000")
  PRINT " Calmar (252)                    = "; FORMAT(METRICS.CalmarRatio(r, 252), "#.000000")
  PRINT " Information ratio               = "; FORMAT(METRICS.InformationRatio(r, b, 252), "#.000000")
  PRINT " Treynor                         = "; FORMAT(METRICS.TreynorRatio(r, b, 0, 252), "#.000000")
  PRINT " max drawdown                    = "; FORMAT(METRICS.MaxDrawdown(r), "#.000000")
  PRINT " VaR 80%                         = "; FORMAT(METRICS.ValueAtRisk(r, 0.80), "#.000000")
  PRINT " CVaR 80%                        = "; FORMAT(METRICS.ConditionalValueAtRisk(r, 0.80), "#.000000")
  PRINT " beta vs benchmark               = "; FORMAT(METRICS.Beta(r, b), "#.000000")
  PRINT " alpha (Jensen, 252)             = "; FORMAT(METRICS.Alpha(r, b, 0, 252), "#.000000")
  PRINT " skewness                        = "; FORMAT(METRICS.ReturnsSkewness(r), "#.000000")
  PRINT " kurtosi (eccesso)               = "; FORMAT(METRICS.ReturnsKurtosis(r), "#.000000")
  jb = METRICS.JarqueBeraTest(r)
  PRINT " Jarque-Bera (stat, p)           = "; FORMAT(jb(0), "#.000000"); ","; FORMAT(jb(1), "#.000000")
  PRINT " tracking error                  = "; FORMAT(METRICS.AnnualizedTrackingError(r, b, 252), "#.000000")
  PRINT " Omega (soglia 0)                = "; FORMAT(METRICS.OmegaRatio(g, 0), "#.000000")
  PRINT " Ulcer index                     = "; FORMAT(METRICS.UlcerIndex(g), "#.000000")

  ' ══════════════════════════════════════════════════════════════════════════
  ' C) VOLATILITÀ
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "C) volatilità"
  PRINT " annualizzata (252)              = "; FORMAT(VOLATILITY.AnnualizedVolatility(r, 252), "#.000000")
  PRINT " downside deviation (soglia 0)   = "; FORMAT(VOLATILITY.DownsideDeviation(r, 0), "#.000000")
  PRINT " downside annualizzata           = "; FORMAT(VOLATILITY.AnnualizedDownsideDeviation(r, 0, 252), "#.000000")

  ' ══════════════════════════════════════════════════════════════════════════
  ' D) FONDAMENTALI
  '    utile 1000, 500 azioni, 200 di dividendi, prezzo 50, book 25, vendite/azione 10
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "D) fondamentali"
  PRINT " EPS (1000/500)                  = "; FORMAT(FUNDAMENTALS.EPS(1000, 500), "#.000000")
  PRINT " EPS diluito (1000/(500+500))    = "; FORMAT(FUNDAMENTALS.DilutedEPS(1000, 500, 500), "#.000000")
  PRINT " DPS (500/500)                   = "; FORMAT(FUNDAMENTALS.DPS(500, 500), "#.000000")
  PRINT " book value per share            = "; FORMAT(FUNDAMENTALS.BookValuePerShare(2000, 500), "#.000000")
  PRINT " NAV per azione                  = "; FORMAT(FUNDAMENTALS.NAV(2000, 500, 500), "#.000000")
  PRINT " equity multiplier (2000/1000)   = "; FORMAT(FUNDAMENTALS.EquityMultiplier(2000, 1000), "#.000000")
  PRINT " payout ratio (200/500)          = "; FORMAT(FUNDAMENTALS.DividendPayoutRatio(200, 500), "#.000000")
  PRINT " dividend yield (5/100)          = "; FORMAT(FUNDAMENTALS.DividendYield(5, 100), "#.000000"); " %"
  PRINT " P/E (50/2)                      = "; FORMAT(FUNDAMENTALS.PERatio(50, 2), "#.000000")
  PRINT " P/B (50/25)                     = "; FORMAT(FUNDAMENTALS.PBRatio(50, 25), "#.000000")
  PRINT " P/S (50/10)                     = "; FORMAT(FUNDAMENTALS.PSRatio(50, 10), "#.000000")

  PRINT "fine"
END SUB
