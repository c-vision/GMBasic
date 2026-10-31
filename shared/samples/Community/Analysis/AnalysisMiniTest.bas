' FILE: Analysis/AnalysisMiniTest.bas
' Dimostra ANALYSIS functions coperte dai MiniTest: AdfTest, Irfft, Pca,
' Rfft, SolveIvp, Splev, Splrep. Output in work/.
IMPORT ANALYSIS
IMPORT MATRIX

SUB Main()
  ' AdfTest su trend lineare (non stazionario)
  DIM d(49) AS DOUBLE
  FOR i = 0 TO 49: d(i) = i: NEXT
  DIM adf AS ANALYSIS.AdfTestResult
  adf = ANALYSIS.AdfTest(d)
  PRINT "adf_stat="; ROUND(adf.Statistic, 4); " p="; ROUND(adf.PValue, 4)
  ' Rfft/Irfft: cos(2pi*i/8)
  DIM x(7) AS DOUBLE
  FOR i = 0 TO 7: x(i) = COS(2 * 3.141592653589793 * i / 8): NEXT
  DIM rf = ANALYSIS.Rfft(x)
  PRINT "rfft_handle="; rf
  DIM re(4) AS DOUBLE: DIM im(4) AS DOUBLE
  FOR k = 0 TO 4: re(k) = MATRIX.GETVALUE(rf, k, 0): im(k) = MATRIX.GETVALUE(rf, k, 1): NEXT
  DIM ir() = ANALYSIS.Irfft(re, im)
  PRINT "irfft0="; ROUND(ir(0), 4)
  ' Pca
  DIM data(3, 1) AS DOUBLE
  FOR i = 0 TO 3: data(i,0) = i + 1: data(i,1) = (i + 1) * 2: NEXT
  DIM p AS ANALYSIS.PcaResult
  p = ANALYSIS.Pca(data)
  PRINT "pca_ev="; ROUND(p.EigenValues(0), 4); ","; ROUND(p.EigenValues(1), 4)
  ' Splrep/Splev: (x+1)^2
  DIM xs(4) AS DOUBLE = {0, 1, 2, 3, 4}
  DIM ys(4) AS DOUBLE = {1, 4, 9, 16, 25}
  DIM sp = ANALYSIS.Splrep(xs, ys)
  DIM xq(0) AS DOUBLE = {2.5}
  DIM sv() = ANALYSIS.Splev(sp, xq)
  PRINT "splev="; ROUND(sv(0), 4)
  ' SolveIvp: dy/dt = -y, y(0)=1
  DIM y0(0) AS DOUBLE = {1}
  DIM n = ANALYSIS.SolveIvp(FN(t, y) => {-y(0)}, y0, 0.0, 1.0)
  PRINT "solveivp_n="; n
END SUB