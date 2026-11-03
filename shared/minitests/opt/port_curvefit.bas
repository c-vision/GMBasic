' EXPECT: LM= 1.0016 , 0.9996|CF= 1.0016 , 0.9996
' Port FFI di optimize/curvefit.bas.
IMPORT OPT
IMPORT MATH
SUB Main()
  DIM DataX(4) AS DOUBLE = {0, 1, 2, 3, 4}
  DIM DataY(4) AS DOUBLE = {1.0, 2.7, 7.4, 20.1, 54.6}
  DIM P0(1) AS DOUBLE = {0.5, 0.5}
  DIM Model = FN(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE => p(0) * EXP(p(1) * x)
  DIM o(1) AS DOUBLE
  CALL OPT.LEVENBERGMARQUARDT(Model, P0, DataX, DataY, o)
  PRINT "LM="; MATH.ROUND(o(0), 4); ","; MATH.ROUND(o(1), 4)
  CALL OPT.CURVEFIT(Model, DataX, DataY, P0, o)
  PRINT "CF="; MATH.ROUND(o(0), 4); ","; MATH.ROUND(o(1), 4)
END SUB
