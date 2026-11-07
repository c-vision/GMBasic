' QUANT (FFI, libgmbquant) — BONDS (9) + DERIVATIVES (16).
' Date = DATETIME.DATEVALUE (OADate); opzioni S=K=100, T=1, r=5%, vol=20%.
' EXPECT: bp0=100.000000||bp1=95.680812||by0=0.072191||by1=0.048775||dur=1.952381||durmod=1.859410||bey=0.030880||cy=0.050000||ytm=0.043902||zcbv=78.352617||zcby=0.040488||ps=100.000000||call=10.450584||put=5.573526||callq=9.227006||mert=9.227006||mertp=6.330081||bin=10.430612||delta=0.636831||deltap=-0.363169||gamma=0.018762||vega=0.375240||theta=-0.017573||rho=0.532325||vanna=-0.281430||volga=9.850059||charm=-0.000253||speed=-0.000516||zomma=-0.088885||color=-0.025798||f=0
IMPORT BONDS
IMPORT DERIVATIVES

SUB Main()
  DIM s AS DOUBLE, m AS DOUBLE
  s = DATETIME.DATEVALUE("2024-01-01")
  m = DATETIME.DATEVALUE("2026-01-01")

  ' ── BONDS ─────────────────────────────────────────────────────────────────
  PRINT "bp0="; FORMAT(BONDS.BondPrice(s, m, 0.05, 0.05, 100, 1), "#.000000")
  PRINT "|bp1="; FORMAT(BONDS.BondPrice(s, m, 0.05, 0.05, 100, 1, 1), "#.000000")
  PRINT "|by0="; FORMAT(BONDS.BondYield(s, m, 0.05, 96, 100, 1), "#.000000")
  PRINT "|by1="; FORMAT(BONDS.BondYield(s, m, 0.05, 96, 100, 1, 1), "#.000000")
  PRINT "|dur="; FORMAT(BONDS.DURATION(s, m, 0.05, 0.05, 1, 0, 0), "#.000000")
  PRINT "|durmod="; FORMAT(BONDS.DURATION(s, m, 0.05, 0.05, 1, 0, 1), "#.000000")
  PRINT "|bey="; FORMAT(BONDS.BondEquivalentYield(100, 98.5, 180), "#.000000")
  PRINT "|cy="; FORMAT(BONDS.CurrentYield(5, 100), "#.000000")
  PRINT "|ytm="; FORMAT(BONDS.YieldToMaturity(5, 100, 105, 10), "#.000000")
  PRINT "|zcbv="; FORMAT(BONDS.ZeroCouponBondValue(100, 0.05, 5), "#.000000")
  PRINT "|zcby="; FORMAT(BONDS.ZeroCouponBondYield(100, 82, 5), "#.000000")
  PRINT "|ps="; FORMAT(BONDS.PRICESIMPLE(s, m, 0.05, 0.05, 100, 1), "#.000000")

  ' ── DERIVATIVES ───────────────────────────────────────────────────────────
  PRINT "|call="; FORMAT(DERIVATIVES.BsCall(100, 100, 1, 0.05, 0.2), "#.000000")
  PRINT "|put="; FORMAT(DERIVATIVES.BsPut(100, 100, 1, 0.05, 0.2), "#.000000")
  PRINT "|callq="; FORMAT(DERIVATIVES.BsCall(100, 100, 1, 0.05, 0.2, 0.02), "#.000000")
  PRINT "|mert="; FORMAT(DERIVATIVES.MertonCall(100, 100, 1, 0.05, 0.2, 0.02), "#.000000")
  PRINT "|mertp="; FORMAT(DERIVATIVES.MertonPut(100, 100, 1, 0.05, 0.2, 0.02), "#.000000")
  PRINT "|bin="; FORMAT(DERIVATIVES.BinomialOption(100, 100, 1, 0.05, 0.2, 100, "CALL"), "#.000000")
  PRINT "|delta="; FORMAT(DERIVATIVES.Delta(100, 100, 1, 0.05, 0.2, "CALL"), "#.000000")
  PRINT "|deltap="; FORMAT(DERIVATIVES.Delta(100, 100, 1, 0.05, 0.2, "PUT"), "#.000000")
  PRINT "|gamma="; FORMAT(DERIVATIVES.Gamma(100, 100, 1, 0.05, 0.2), "#.000000")
  PRINT "|vega="; FORMAT(DERIVATIVES.Vega(100, 100, 1, 0.05, 0.2), "#.000000")
  PRINT "|theta="; FORMAT(DERIVATIVES.Theta(100, 100, 1, 0.05, 0.2, "CALL"), "#.000000")
  PRINT "|rho="; FORMAT(DERIVATIVES.Rho(100, 100, 1, 0.05, 0.2, "CALL"), "#.000000")
  PRINT "|vanna="; FORMAT(DERIVATIVES.Vanna(100, 100, 1, 0.05, 0.2), "#.000000")
  PRINT "|volga="; FORMAT(DERIVATIVES.Volga(100, 100, 1, 0.05, 0.2), "#.000000")
  PRINT "|charm="; FORMAT(DERIVATIVES.Charm(100, 100, 1, 0.05, 0.2, "CALL"), "#.000000")
  PRINT "|speed="; FORMAT(DERIVATIVES.DeltaSpeed(100, 100, 1, 0.05, 0.2), "#.000000")
  PRINT "|zomma="; FORMAT(DERIVATIVES.Zomma(100, 100, 1, 0.05, 0.2), "#.000000")
  PRINT "|color="; FORMAT(DERIVATIVES.GammaColor(100, 100, 1, 0.05, 0.2), "#.000000")
  PRINT "|f=0"
END SUB
