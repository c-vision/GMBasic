' scipy.signal P0 (firwin/find_peaks) e scipy.integrate P0 (solve_ivp RK45 adattivo).
' Oracoli: scipy 1.18.
IMPORT SIGNAL
IMPORT ANALYSIS
IMPORT MATRIX

SUB Main()
  ' firwin(11, 0.2, 'lowpass', 'hann') → tap centrale 0.4 (DC gain 2·fc)
  DIM fc(0) AS DOUBLE = {0.2}
  DIM taps() = SIGNAL.FIRWIN(11, fc, "lowpass", "hann")
  PRINT "firwin5  ="; taps(5)
  PRINT "firwin4  ="; taps(4)

  ' find_peaks: [0,1,0,3,0,2,0] → picchi 1,3,5
  DIM sig(6) AS DOUBLE = {0, 1, 0, 3, 0, 2, 0}
  DIM pk() = SIGNAL.FINDPEAKS(sig, 1)
  PRINT "peaks    ="; pk(0); pk(1); pk(2)

  ' solve_ivp: y' = y, y(0)=1 → y(1) = e ≈ 2.71828
  DIM y0(0) AS DOUBLE = {1}
  h = ANALYSIS.SOLVEIVP(FN(t AS DOUBLE, y() AS DOUBLE) AS ARRAY OF DOUBLE => {y(0)}, y0, 0, 1)
  n = MATRIX.ROWCOUNT(h)
  PRINT "ivprows  ="; n
  PRINT "ivpyend  ="; MATRIX.GETVALUE(h, n - 1, 1)
END SUB
