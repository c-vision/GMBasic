' EXPECT: x0= 1.044209|x1=-0.4536|f= 0.209899
IMPORT SIMULATION
FUNCTION Obj(x() AS DOUBLE) AS DOUBLE
  RETURN (x(0) - 1.5) * (x(0) - 1.5) + (x(1) + 0.5) * (x(1) + 0.5)
END FUNCTION
SUB Main()
  DIM lo(1) AS DOUBLE
  DIM hi(1) AS DOUBLE
  DIM out(1) AS DOUBLE
  lo(0) = -5: lo(1) = -5
  hi(0) = 5: hi(1) = 5
  fb = SIMULATION.TPE(Obj, lo, hi, 20, 8, 12345, out)
  PRINT "x0="; MATH.ROUND(out(0), 6)
  PRINT "x1="; MATH.ROUND(out(1), 6)
  PRINT "f="; MATH.ROUND(fb, 6)
END SUB
