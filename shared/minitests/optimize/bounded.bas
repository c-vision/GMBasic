' EXPECT: tnc= 1   1|cobyla=-0.958   0.926|slsqp= 1   1|minimize= 1   1|brute= 1.05  -1.95|de= 1  -2|dual= 1  -2
IMPORT OPTIMIZE
IMPORT MATH
SUB Main()
  DIM x0(1) AS DOUBLE = {-1.2, 1.0}
  DIM bd(1) AS ARRAY OF DOUBLE
  bd(0) = {-5.0, 5.0} : bd(1) = {-5.0, 5.0}
  DIM f = FN(p() AS DOUBLE) AS DOUBLE => (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) * p(0)) ^ 2
  DIM tn() = OPTIMIZE.TNC(f, x0, bd)
  PRINT "tnc="; MATH.ROUND(tn(0), 3); " "; MATH.ROUND(tn(1), 3)
  DIM co() = OPTIMIZE.COBYLA(f, x0, bd)
  PRINT "cobyla="; MATH.ROUND(co(0), 3); " "; MATH.ROUND(co(1), 3)
  DIM sq() = OPTIMIZE.SLSQP(f, x0, bd)
  PRINT "slsqp="; MATH.ROUND(sq(0), 3); " "; MATH.ROUND(sq(1), 3)
  DIM mn() = OPTIMIZE.MINIMIZE(f, x0, "powell", bd)
  PRINT "minimize="; MATH.ROUND(mn(0), 3); " "; MATH.ROUND(mn(1), 3)
  DIM rg(1) AS ARRAY OF DOUBLE
  rg(0) = {-3.0, 3.0} : rg(1) = {-3.0, 3.0}
  DIM g = FN(p() AS DOUBLE) AS DOUBLE => (p(0) - 1) ^ 2 + (p(1) + 2) ^ 2
  DIM br() = OPTIMIZE.BRUTE(g, rg, 41)
  PRINT "brute="; MATH.ROUND(br(0), 3); " "; MATH.ROUND(br(1), 3)
  DIM de() = OPTIMIZE.DIFFERENTIALEVOLUTION(g, rg)
  PRINT "de="; MATH.ROUND(de(0), 2); " "; MATH.ROUND(de(1), 2)
  DIM da() = OPTIMIZE.DUALANNEALING(g, rg, 200)
  PRINT "dual="; MATH.ROUND(da(0), 2); " "; MATH.ROUND(da(1), 2)
END SUB
