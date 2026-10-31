' scipy P1: welch, savgol_filter, resample, decimate, splrep/splev, pdist/cdist,
' distance_matrix, rfft/irfft, ConvexHull. Oracoli: scipy 1.18.
IMPORT SIGNAL
IMPORT ANALYSIS
IMPORT SPATIAL
IMPORT MATRIX

SUB Main()
  DIM n(255) AS DOUBLE
  FOR i = 0 TO 255 : n(i) = SIN(2 * 3.141592653589793 * 0.1 * i) : NEXT

  ' welch: picco PSD vicino a f=0.1
  w = SIGNAL.WELCH(n, 64, 1.0)
  PRINT "welchrows ="; MATRIX.ROWCOUNT(w)
  PRINT "welch00   ="; MATRIX.GETVALUE(w, 0, 0)

  ' savgol su una parabola → esatto
  DIM q(19) AS DOUBLE
  FOR i = 0 TO 19 : q(i) = i * i : NEXT
  DIM sg() = SIGNAL.SAVGOLFILTER(q, 9, 2)
  PRINT "savgol10  ="; sg(10)   ' 100
  PRINT "savgol0   ="; sg(0)    ' 0

  ' resample 256→128 di un seno
  DIM rs() = SIGNAL.RESAMPLE(n, 128)
  PRINT "resample64="; rs(64)

  ' decimate q=4
  DIM dc() = SIGNAL.DECIMATE(n, 4)
  PRINT "decimate0 ="; dc(0)

  ' splrep/splev: (x+1)^2 → a 2.5 = 12.25
  DIM xs(4) AS DOUBLE = {0, 1, 2, 3, 4}
  DIM ys(4) AS DOUBLE = {1, 4, 9, 16, 25}
  sp = ANALYSIS.SPLREP(xs, ys)
  DIM xq(0) AS DOUBLE = {2.5}
  DIM sv() = ANALYSIS.SPLEV(sp, xq)
  PRINT "splev     ="; sv(0)

  ' pdist/cdist/distance_matrix
  DIM X(2) AS ARRAY OF DOUBLE
  X(0) = {0, 0} : X(1) = {3, 4} : X(2) = {6, 8}
  DIM Y(0) AS ARRAY OF DOUBLE
  Y(0) = {0, 0}
  DIM pd() = SPATIAL.PDIST(X, "euclidean")
  PRINT "pdist0    ="; pd(0)
  cdm = SPATIAL.CDIST(X, Y, "euclidean")
  PRINT "cdist10   ="; MATRIX.GETVALUE(cdm, 1, 0)
  dm = SPATIAL.DISTANCEMATRIX(X, Y)
  PRINT "distmat10 ="; MATRIX.GETVALUE(dm, 1, 0)

  ' rfft/irfft: cos(2πi/8), ricostruzione
  DIM x2(7) AS DOUBLE
  FOR i = 0 TO 7 : x2(i) = COS(2 * 3.141592653589793 * i / 8) : NEXT
  rf = ANALYSIS.RFFT(x2)
  PRINT "rfft10    ="; MATRIX.GETVALUE(rf, 1, 0)
  DIM re(4) AS DOUBLE
  DIM im(4) AS DOUBLE
  FOR k = 0 TO 4
    re(k) = MATRIX.GETVALUE(rf, k, 0) : im(k) = MATRIX.GETVALUE(rf, k, 1)
  NEXT
  DIM ir() = ANALYSIS.IRFFT(re, im)
  PRINT "irfft0    ="; ir(0)

  ' ConvexHull quadrato
  DIM pts(4) AS ARRAY OF DOUBLE
  pts(0) = {0, 0} : pts(1) = {1, 0} : pts(2) = {1, 1} : pts(3) = {0, 1} : pts(4) = {0.5, 0.5}
  DIM hull() = SPATIAL.CONVEXHULL(pts)
  PRINT "hull      ="; hull(0); hull(1); hull(2); hull(3)
END SUB
