IMPORT BONDS AS B
SUB Main()
  PRINT ROUND(B.BondEquivalentYield(100, 96, 365), 6)
  PRINT ROUND(B.CurrentYield(5, 96), 6)
  PRINT ROUND(B.YieldToMaturity(5, 100, 96, 10), 6)
  PRINT ROUND(B.ZeroCouponBondValue(100, 0.05, 5), 6)
  PRINT ROUND(B.ZeroCouponBondYield(100, 80, 5), 6)
END SUB
