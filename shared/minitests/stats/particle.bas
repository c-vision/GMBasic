' EXPECT: m0= 1.091251|m1= 1.137169|m19= 1.384645|v0= 0.046334|v19= 0.063281
IMPORT SIMULATION
SUB Main()
  DIM obs(19) AS DOUBLE
  FOR i = 0 TO 19
    obs(i) = 1.0 + 0.5 * SIN(i * 0.4) + 0.1 * COS(i * 1.3)
  NEXT i
  DIM p = SIMULATION.PARTICLEFILTER(obs, 0.9, 0.1, 0.1, 200, 12345)
  PRINT "m0="; MATH.ROUND(p.Mean(0), 6)
  PRINT "m1="; MATH.ROUND(p.Mean(1), 6)
  PRINT "m19="; MATH.ROUND(p.Mean(19), 6)
  PRINT "v0="; MATH.ROUND(p.Variance(0), 6)
  PRINT "v19="; MATH.ROUND(p.Variance(19), 6)
END SUB
