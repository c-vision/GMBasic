' EXPECT: m0= 1.112451|m1= 1.102708|m19= 1.325186|v0= 0.048296|v19= 0.051485
IMPORT SIMULATION
SUB Main()
  DIM obs(19) AS DOUBLE
  DIM m(19) AS DOUBLE
  DIM v(19) AS DOUBLE
  DIM par(2) AS DOUBLE
  par(0) = 0.9: par(1) = 0.1: par(2) = 0.1
  FOR i = 0 TO 19
    obs(i) = 1.0 + 0.5 * SIN(i * 0.4) + 0.1 * COS(i * 1.3)
  NEXT i
  SIMULATION.PARTICLEFILTER(obs, 20, par, 200, 12345, m, v)
  PRINT "m0="; MATH.ROUND(m(0), 6)
  PRINT "m1="; MATH.ROUND(m(1), 6)
  PRINT "m19="; MATH.ROUND(m(19), 6)
  PRINT "v0="; MATH.ROUND(v(0), 6)
  PRINT "v19="; MATH.ROUND(v(19), 6)
END SUB
