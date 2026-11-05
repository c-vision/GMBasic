' EXPECT: x0= 1.145718|x1=-0.429787|f= 0.130445|l0=-1.078609|l1=-0.125512|lf= 0.021932
IMPORT SIMULATION
FUNCTION Obj(x() AS DOUBLE) AS DOUBLE
  RETURN (x(0) - 1.5) * (x(0) - 1.5) + (x(1) + 0.5) * (x(1) + 0.5)
END FUNCTION
SUB Main()
  DIM lo(1) AS DOUBLE
  DIM hi(1) AS DOUBLE
  lo(0) = -5: lo(1) = -5
  hi(0) = 5: hi(1) = 5
  DIM r() = SIMULATION.TPE(Obj, lo, hi, 20, 8, 12345)
  PRINT "x0="; MATH.ROUND(r(0), 6)
  PRINT "x1="; MATH.ROUND(r(1), 6)
  PRINT "f="; MATH.ROUND(r(2), 6)
  DIM rl() = SIMULATION.TPE({ |x| RETURN (x(0) + 1.0) * (x(0) + 1.0) + x(1) * x(1) }, lo, hi, 20, 8, 777)
  PRINT "l0="; MATH.ROUND(rl(0), 6)
  PRINT "l1="; MATH.ROUND(rl(1), 6)
  PRINT "lf="; MATH.ROUND(rl(2), 6)
END SUB
