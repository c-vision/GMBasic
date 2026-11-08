' MiniTest NAMESPACE TRADING — tutte le 12 funzioni della superficie.
' Derivato da shared/minitests/trading/stock.bas (stessi input e oracoli).
' EXPECT: 20|33.3333|25|0.114|0.057143|0.09|0.15|0.1|0.09|1|0.254|0.078365
IMPORT TRADING

SUB Main()
  PRINT TRADING.StockValueZeroGrowth(2, 0.1)
  PRINT ROUND(TRADING.StockValueConstGrowth(2, 0.1, 0.04), 4)
  PRINT TRADING.PreferredStockValue(2, 0.08)
  PRINT ROUND(TRADING.CAPM(0.03, 1.2, 0.1), 6)
  PRINT ROUND(TRADING.TaxEquivalentYield(0.04, 0.3), 6)
  PRINT ROUND(TRADING.RiskPremium(0.12, 0.03), 6)
  PRINT ROUND(TRADING.TotalStockReturn(100, 110, 5), 6)
  PRINT ROUND(TRADING.CapitalGainsYield(100, 110), 6)
  PRINT ROUND(TRADING.GrowthRate(0.6, 0.15), 6)
  PRINT ROUND(TRADING.BidAskSpread(100, 101), 6)
  DIM ret(2) AS DOUBLE = {0.1, -0.05, 0.2}
  PRINT ROUND(TRADING.HoldingPeriodReturn(ret), 6)
  PRINT ROUND(TRADING.GeometricMeanReturn(ret), 6)
END SUB
