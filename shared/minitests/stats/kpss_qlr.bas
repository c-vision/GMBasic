' EXPECT: kstat= 0.095901|kbw= 3|kcrit5= 0.146|qt= 7|qtl= 3.5|qbest= 1
IMPORT TIMESERIES
SUB Main()
  DIM x(49) AS DOUBLE
  FOR i = 0 TO 49
    x(i) = 0.5 * SIN(i * 0.3) + 0.1 * COS(i * 0.9) + i * 0.02
  NEXT i
  DIM k = TIMESERIES.KPSS(x, -1, 1)
  PRINT "kstat="; MATH.ROUND(k.Statistic, 6)
  PRINT "kbw="; k.Bandwidth
  PRINT "kcrit5="; k.Critical5
  DIM lb(3) AS DOUBLE
  lb(0) = -10.0: lb(1) = -8.5: lb(2) = -9.2: lb(3) = -11.0
  DIM q = TIMESERIES.QLR(-12.0, lb, 2.0)
  PRINT "qt="; MATH.ROUND(q.QLRT, 6)
  PRINT "qtl="; MATH.ROUND(q.QLRTilde, 6)
  PRINT "qbest="; q.BestIndex
END SUB
