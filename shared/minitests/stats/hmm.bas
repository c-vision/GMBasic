' EXPECT: fwd= -4.723182|vll= -3.247275|p0= 0 p1= 1 p2= 1|pi0= 0|a00= 0.893468|m00= 0.940789|c00= 0.087935
' HMM: forward/Viterbi/fit tutti via TIMESERIES (FFI, libgmbstats).
IMPORT TIMESERIES
SUB Main()
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
  DIM path(2) AS LONG
  PRINT "vll="; MATH.ROUND(TIMESERIES.HMMVITERBI(lp, la, le, path), 6)
  PRINT "p0="; path(0); "p1="; path(1); "p2="; path(2)
  DIM x(19) AS ARRAY OF DOUBLE
  FOR i = 0 TO 19
    x(i) = {SIN(i * 0.35) + i * 0.05, COS(i * 0.35) * 0.8}
  NEXT i
  ' out: pi(2), A(4), means(4), covs(8)
  DIM o(17) AS DOUBLE
  CALL TIMESERIES.HMMFIT(x, 2, 8, o)
  PRINT "pi0="; MATH.ROUND(o(0), 6)
  PRINT "a00="; MATH.ROUND(o(2), 6)
  PRINT "m00="; MATH.ROUND(o(6), 6)
  PRINT "c00="; MATH.ROUND(o(10), 6)
END SUB
