' EXPECT: g= 0.069121|t= 0.0601|c= 0.352941|gu= 0.350266|f= 0.294431
IMPORT MULTIVARIATE
SUB Main()
  DIM x(1) AS DOUBLE
  DIM loc(1) AS DOUBLE
  DIM cov(3) AS DOUBLE
  x(0) = 1: x(1) = 0.5
  loc(0) = 0: loc(1) = 0
  cov(0) = 1: cov(1) = 0.3
  cov(2) = 0.3: cov(3) = 2
  PRINT "g="; MATH.ROUND(MULTIVARIATE.MvGaussPdf(x, loc, cov), 6)
  PRINT "t="; MATH.ROUND(MULTIVARIATE.MvStudentPdf(x, loc, cov, 5), 6)
  PRINT "c="; MATH.ROUND(MULTIVARIATE.CopulaClaytonCdf(0.4, 0.6, 2), 6)
  PRINT "gu="; MATH.ROUND(MULTIVARIATE.CopulaGumbelCdf(0.4, 0.6, 2), 6)
  PRINT "f="; MATH.ROUND(MULTIVARIATE.CopulaFrankCdf(0.4, 0.6, 2), 6)
END SUB
