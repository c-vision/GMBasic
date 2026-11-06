' EXPECT: c0= 1|c1= 2|c2= 3|ssq= 0|pe= 10.75|cheb= 1|pade= 2.2|r2= 0.99781|ang= 0.494723|r2t= 0.696959
IMPORT FITTING
SUB Main()
  DIM x(5) AS DOUBLE
  DIM y(5) AS DOUBLE
  DIM coef(2) AS DOUBLE
  FOR i = 0 TO 5
    x(i) = i * 0.5
    y(i) = 1.0 + 2.0 * x(i) + 3.0 * x(i) * x(i)
  NEXT i
  ssq = FITTING.POLYFIT(x, y, coef)
  PRINT "c0="; MATH.ROUND(coef(0), 6)
  PRINT "c1="; MATH.ROUND(coef(1), 6)
  PRINT "c2="; MATH.ROUND(coef(2), 6)
  PRINT "ssq="; MATH.ROUND(ssq, 8)
  PRINT "pe="; MATH.ROUND(FITTING.POLYEVAL(1.5, coef), 6)
  DIM ch(3) AS DOUBLE
  ch(0) = 1.0: ch(1) = 0.5: ch(2) = 0.25: ch(3) = 0.125
  PRINT "cheb="; MATH.ROUND(FITTING.CHEBEVAL(0.5, ch), 6)
  DIM pa(2) AS DOUBLE
  DIM pb(1) AS DOUBLE
  pa(0) = 1.0: pa(1) = 2.0: pa(2) = 3.0
  pb(0) = 1.0: pb(1) = 0.5
  PRINT "pade="; MATH.ROUND(FITTING.PADEEVAL(0.5, pa, pb), 6)
  DIM d(9) AS DOUBLE
  DIM out(1) AS DOUBLE
  FOR i = 0 TO 9
    d(i) = 1.0 + 0.5 * i + 0.1 * SIN(i * 2.0)
  NEXT i
  FITTING.RSQUARED(d, out)
  PRINT "r2="; MATH.ROUND(out(0), 6)
  PRINT "ang="; MATH.ROUND(out(1), 6)
  PRINT "r2t="; MATH.ROUND(FITTING.RSQUAREDTEST(d), 6)
END SUB
