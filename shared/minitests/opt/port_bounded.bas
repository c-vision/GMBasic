' EXPECT: tnc= 1   1|cobyla=-0.958   0.926|slsqp= 1   1|minimize= 1   1|brute= 1.05  -1.95|de= 1  -2|dual= 1  -2
' Port FFI di optimize/bounded.bas (stesso EXPECT del builtin).
IMPORT OPTIMIZE
SUB Main()
  DIM x0(1) AS DOUBLE = {-1.2, 1.0}
  DIM bd(3) AS DOUBLE
  bd(0) = -5.0 : bd(1) = 5.0 : bd(2) = -5.0 : bd(3) = 5.0
  DIM f = FN(p() AS DOUBLE) AS DOUBLE => (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) * p(0)) ^ 2
  DIM o(1) AS DOUBLE
  CALL OPTIMIZE.TNCB(f, x0, bd, o)
  PRINT "tnc="; MATH.ROUND(o(0), 3); " "; MATH.ROUND(o(1), 3)
  CALL OPTIMIZE.COBYLAB(f, x0, bd, o)
  PRINT "cobyla="; MATH.ROUND(o(0), 3); " "; MATH.ROUND(o(1), 3)
  CALL OPTIMIZE.SLSQPB(f, x0, bd, o)
  PRINT "slsqp="; MATH.ROUND(o(0), 3); " "; MATH.ROUND(o(1), 3)
  CALL OPTIMIZE.MINIMIZEB(f, x0, "powell", bd, o)
  PRINT "minimize="; MATH.ROUND(o(0), 3); " "; MATH.ROUND(o(1), 3)
  DIM rg(3) AS DOUBLE
  rg(0) = -3.0 : rg(1) = 3.0 : rg(2) = -3.0 : rg(3) = 3.0
  DIM g = FN(p() AS DOUBLE) AS DOUBLE => (p(0) - 1) ^ 2 + (p(1) + 2) ^ 2
  DIM o2(1) AS DOUBLE
  CALL OPTIMIZE.BRUTEN(g, rg, 41, o2)
  PRINT "brute="; MATH.ROUND(o2(0), 3); " "; MATH.ROUND(o2(1), 3)
  CALL OPTIMIZE.DIFFERENTIALEVOLUTION(g, rg, o2)
  PRINT "de="; MATH.ROUND(o2(0), 2); " "; MATH.ROUND(o2(1), 2)
  CALL OPTIMIZE.DUALANNEALINGN(g, rg, 200, o2)
  PRINT "dual="; MATH.ROUND(o2(0), 2); " "; MATH.ROUND(o2(1), 2)
END SUB
