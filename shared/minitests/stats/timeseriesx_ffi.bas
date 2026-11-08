' EXPECT: kstat= 0.105418|kbw= 3|kcrit5= 0.146|dstat=-1.874815|dobs= 58|qt= 7|qtl= 3.5|qbest= 1|fwd=-4.723182
IMPORT TIMESERIES
SUB Main()
  DIM x(59) AS DOUBLE
  DIM outk(8) AS DOUBLE
  DIM outd(4) AS DOUBLE
  FOR i = 0 TO 59
    x(i) = 0.4 * SIN(i * 0.25) + 0.1 * COS(i * 0.7) + i * 0.03
  NEXT i
  TIMESERIES.KPSS(x, -1, 1, outk)
  PRINT "kstat="; MATH.ROUND(outk(0), 6)
  PRINT "kbw="; outk(1)
  PRINT "kcrit5="; outk(6)
  TIMESERIES.DFGLS(x, 1, 0, outd)
  PRINT "dstat="; MATH.ROUND(outd(0), 6)
  PRINT "dobs="; outd(2)
  DIM lb(3) AS DOUBLE
  DIM outq(2) AS DOUBLE
  lb(0) = -10.0: lb(1) = -8.5: lb(2) = -9.2: lb(3) = -11.0
  TIMESERIES.QLR(-12.0, lb, 2.0, outq)
  PRINT "qt="; MATH.ROUND(outq(0), 6)
  PRINT "qtl="; MATH.ROUND(outq(1), 6)
  PRINT "qbest="; outq(2)
  DIM lp(1) AS DOUBLE
  DIM la(3) AS DOUBLE
  DIM le(5) AS DOUBLE
  lp(0) = -0.5108256237659907: lp(1) = -0.916290731874155
  la(0) = -0.3566749439387324: la(1) = -1.2039728043259361
  la(2) = -0.916290731874155: la(3) = -0.5108256237659907
  le(0) = -0.1053605156578263: le(1) = -1.6094379124341003
  le(2) = -2.302585092994046: le(3) = -0.2231435513142097
  le(4) = -0.6931471805599453: le(5) = -0.6931471805599453
  PRINT "fwd="; MATH.ROUND(TIMESERIES.HMMFORWARD(lp, la, le), 6)
END SUB
