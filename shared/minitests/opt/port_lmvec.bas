' EXPECT: LMV= 1.0016 , 0.9996
' Port FFI di optimize/lmvec.bas.
IMPORT OPTIMIZE
SUB Main()
  DIM DataX(4) AS DOUBLE = {0, 1, 2, 3, 4}
  DIM DataY(4) AS DOUBLE = {1.0, 2.7, 7.4, 20.1, 54.6}
  DIM P0(1) AS DOUBLE = {0.5, 0.5}
  DIM Model = FN(p() AS DOUBLE, x() AS DOUBLE) AS ARRAY OF DOUBLE => {p(0) * EXP(p(1) * x(0)), p(0) * EXP(p(1) * x(1)), p(0) * EXP(p(1) * x(2)), p(0) * EXP(p(1) * x(3)), p(0) * EXP(p(1) * x(4))}
  DIM o(1) AS DOUBLE
  CALL OPTIMIZE.LEVENBERGMARQUARDTVEC(Model, P0, DataX, DataY, o)
  PRINT "LMV="; MATH.ROUND(o(0), 4); ","; MATH.ROUND(o(1), 4)
END SUB
