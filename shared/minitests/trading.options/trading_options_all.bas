' MiniTest NAMESPACE TRADING.OPTIONS — tutte le 16 funzioni della superficie.
' BsCall/BsPut/Delta/Gamma/Vega/Rho + BinomialOption/Charm/DeltaSpeed/GammaColor/
' MertonCall/MertonPut/Theta/Vanna/Volga/Zomma.
' EXPECT: 10.4506|5.5735|0.636831|0.01876202|0.37524035|0.53232482|10.430612|-0.000253|-0.000516|-0.025798|10.450584|5.573526|-0.017573|-0.28143|9.850059|-0.088885
IMPORT DERIVATIVES

SUB O_options()
  PRINT ROUND(DERIVATIVES.BsCall(100, 100, 1, 0.05, 0.2, 0), 4)
  PRINT ROUND(DERIVATIVES.BsPut(100, 100, 1, 0.05, 0.2, 0), 4)
  PRINT ROUND(DERIVATIVES.Delta(100, 100, 1, 0.05, 0.2, "CALL"), 6)
  PRINT ROUND(DERIVATIVES.Gamma(100, 100, 1, 0.05, 0.2), 8)
  PRINT ROUND(DERIVATIVES.Vega(100, 100, 1, 0.05, 0.2), 8)
  PRINT ROUND(DERIVATIVES.Rho(100, 100, 1, 0.05, 0.2, "CALL"), 8)
END SUB

SUB O_binomialoption()
  PRINT ROUND(DERIVATIVES.BinomialOption(100, 100, 1, 0.05, 0.2, 100, "CALL"), 6)
END SUB

SUB O_charm()
  PRINT ROUND(DERIVATIVES.Charm(100, 100, 1, 0.05, 0.2, "CALL"), 6)
END SUB

SUB O_deltaspeed()
  PRINT ROUND(DERIVATIVES.DeltaSpeed(100, 100, 1, 0.05, 0.2), 6)
END SUB

SUB O_gammacolor()
  PRINT ROUND(DERIVATIVES.GammaColor(100, 100, 1, 0.05, 0.2), 6)
END SUB

SUB O_mertoncall()
  PRINT ROUND(DERIVATIVES.MertonCall(100, 100, 1, 0.05, 0.2, 0), 6)
END SUB

SUB O_mertonput()
  PRINT ROUND(DERIVATIVES.MertonPut(100, 100, 1, 0.05, 0.2, 0), 6)
END SUB

SUB O_theta()
  PRINT ROUND(DERIVATIVES.Theta(100, 100, 1, 0.05, 0.2, "CALL"), 6)
END SUB

SUB O_vanna()
  PRINT ROUND(DERIVATIVES.Vanna(100, 100, 1, 0.05, 0.2), 6)
END SUB

SUB O_volga()
  PRINT ROUND(DERIVATIVES.Volga(100, 100, 1, 0.05, 0.2), 6)
END SUB

SUB O_zomma()
  PRINT ROUND(DERIVATIVES.Zomma(100, 100, 1, 0.05, 0.2), 6)
END SUB

SUB Main()
  O_options()
  O_binomialoption()
  O_charm()
  O_deltaspeed()
  O_gammacolor()
  O_mertoncall()
  O_mertonput()
  O_theta()
  O_vanna()
  O_volga()
  O_zomma()
END SUB
