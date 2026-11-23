' FILE: Analysis/AnalysisMiniTest.bas
' Dimostra ANALYSIS functions coperte dai MiniTest: AdfTest, Irfft, Pca,
' Rfft, SolveIvp, Splev, Splrep. Output in work/.

IMPORT MULTIVARIATE
IMPORT TIMESERIES
IMPORT SIGNAL
IMPORT INTERP
FUNCTION Dy(t AS DOUBLE, y() AS DOUBLE) AS ARRAY OF DOUBLE
  RETURN {-y(0)}
END FUNCTION
SUB Main()
  ' AdfTest su trend lineare (non stazionario)
  DIM d(49) AS DOUBLE
  FOR i = 0 TO 49: d(i) = i: NEXT
  DIM adf() AS DOUBLE = TIMESERIES.ADFTEST(d)
  PRINT "adf_stat="; ROUND(adf(0), 4); " p="; ROUND(adf(1), 4)
  ' Rfft/Irfft: cos(2pi*i/8) — RFFT ritorna [re0, im0, re1, im1, …]
  DIM x(7) AS DOUBLE
  FOR i = 0 TO 7: x(i) = COS(2 * 3.141592653589793 * i / 8): NEXT
  DIM rf() = SIGNAL.RFFT(x)
  PRINT "rfft_len="; LEN(rf)
  DIM re(4) AS DOUBLE: DIM im(4) AS DOUBLE
  FOR k = 0 TO 4: re(k) = rf(2 * k): im(k) = rf(2 * k + 1): NEXT
  DIM ir() = SIGNAL.IRFFT(re, im)
  PRINT "irfft0="; ROUND(ir(0), 4)
  ' Pca
  DIM data(3, 1) AS DOUBLE
  FOR i = 0 TO 3: data(i,0) = i + 1: data(i,1) = (i + 1) * 2: NEXT
  DIM po(5) AS DOUBLE
  CALL MULTIVARIATE.PCA(data, po)
  PRINT "pca_ev="; ROUND(po(0), 4); ","; ROUND(po(1), 4)
  ' Splrep/Splev: (x+1)^2
  DIM xs(4) AS DOUBLE = {0, 1, 2, 3, 4}
  DIM ys(4) AS DOUBLE = {1, 4, 9, 16, 25}
  DIM sp() = INTERP.SPLREP(xs, ys)
  DIM xq(0) AS DOUBLE = {2.5}
  DIM sv() = INTERP.SPLEV(sp, xq)
  PRINT "splev="; ROUND(sv(0), 4)
  ' SolveIvp: dy/dt = -y, y(0)=1
  DIM y0(0) AS DOUBLE = {1}
  DIM n = NUMERICAL.SolveIvp(Dy, y0, 0.0, 1.0)
  PRINT "solveivp_n="; n
END SUB