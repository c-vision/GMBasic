' EXPECT: kstat= 0.095901|kbw= 3|kcrit5= 0.146|qt= 7|qtl= 3.5|qbest= 1
' TIMESERIES (FFI, libgmbstats): dimensioni dedotte, uscite in out() piatto.
IMPORT TIMESERIESX
SUB Main()
  DIM x(49) AS DOUBLE
  FOR i = 0 TO 49
    x(i) = 0.5 * SIN(i * 0.3) + 0.1 * COS(i * 0.9) + i * 0.02
  NEXT i
  ' out(9): statistic, bandwidth, observations, deterministic, lrv, crit10/5/25/1
  DIM outk(8) AS DOUBLE
  TIMESERIESX.KPSS(x, -1, 1, outk)
  PRINT "kstat="; MATH.ROUND(outk(0), 6)
  PRINT "kbw="; outk(1)
  PRINT "kcrit5="; outk(6)
  DIM lb(3) AS DOUBLE
  lb(0) = -10.0: lb(1) = -8.5: lb(2) = -9.2: lb(3) = -11.0
  ' out(3): qlr_t, qlr_tilde, best_index
  DIM outq(2) AS DOUBLE
  TIMESERIESX.QLR(-12.0, lb, 2.0, outq)
  PRINT "qt="; MATH.ROUND(outq(0), 6)
  PRINT "qtl="; MATH.ROUND(outq(1), 6)
  PRINT "qbest="; outq(2)
END SUB
