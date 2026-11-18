' EXPECT: 20|33.3333|25|0.114|0.057143|0.09|0.15|0.1|0.09|1|0.254|0.078365
IMPORT TRADING AS T
SUB Main()
  PRINT T.StockValueZeroGrowth(2, 0.1)
  PRINT ROUND(T.StockValueConstGrowth(2, 0.1, 0.04), 4)
  PRINT T.PreferredStockValue(2, 0.08)
  PRINT ROUND(T.CAPM(0.03, 1.2, 0.1), 6)
  PRINT ROUND(T.TaxEquivalentYield(0.04, 0.3), 6)
  PRINT ROUND(T.RiskPremium(0.12, 0.03), 6)
  PRINT ROUND(T.TotalStockReturn(100, 110, 5), 6)
  PRINT ROUND(T.CapitalGainsYield(100, 110), 6)
  PRINT ROUND(T.GrowthRate(0.6, 0.15), 6)
  PRINT ROUND(T.BidAskSpread(100, 101), 6)
  DIM ret(2) AS DOUBLE = {0.1, -0.05, 0.2}
  PRINT ROUND(T.HoldingPeriodReturn(ret), 6)
  PRINT ROUND(T.GeometricMeanReturn(ret), 6)
END SUB
