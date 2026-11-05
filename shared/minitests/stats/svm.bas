' EXPECT: w0=-0.416667|w1=-0.375|w2= 0.416667|cls= 2|p0= 0|p1= 1
IMPORT MULTIVARIATE
SUB Main()
  DIM X(11, 1) AS DOUBLE
  DIM y(11) AS DOUBLE
  FOR i = 0 TO 5
    X(i, 0) = 2 + 0.2 * i: X(i, 1) = 2 + 0.1 * i
    y(i) = 0
  NEXT i
  FOR i = 6 TO 11
    X(i, 0) = 8 + 0.2 * i: X(i, 1) = 8 + 0.1 * i
    y(i) = 1
  NEXT i
  DIM r = MULTIVARIATE.SVMFIT(X, y, 2, 1.0, 100)
  PRINT "w0="; MATH.ROUND(r.W(0), 6)
  PRINT "w1="; MATH.ROUND(r.W(1), 6)
  PRINT "w2="; MATH.ROUND(r.W(2), 6)
  PRINT "cls="; r.Classes
  DIM wm(3) AS DOUBLE
  wm(0) = 0: wm(1) = 0: wm(2) = 1: wm(3) = -1
  DIM q(1) AS DOUBLE
  q(0) = 2.0: q(1) = 2.0
  PRINT "p0="; MULTIVARIATE.SVMPREDICT(wm, q, 2, 2)
  q(0) = 8.0: q(1) = 7.6
  PRINT "p1="; MULTIVARIATE.SVMPREDICT(wm, q, 2, 2)
END SUB
