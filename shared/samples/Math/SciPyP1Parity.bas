' scipy P1: welch, savgol_filter, resample, decimate, splrep/splev, pdist/cdist,
' distance_matrix, rfft/irfft, ConvexHull. Oracoli: scipy 1.18.
IMPORT SIGNAL
IMPORT SPATIAL
IMPORT INTERP

SUB Main()
  DIM n(255) AS DOUBLE
  FOR i = 0 TO 255 : n(i) = SIN(2 * 3.141592653589793 * 0.1 * i) : NEXT

  ' welch: picco PSD vicino a f=0.1 (array interleaved [freq, psd, …] dal pacchetto)
  DIM w() = SIGNAL.WELCH(n, 64, 1.0)
  PRINT "welchrows ="; LEN(w) \ 2
  PRINT "welch00   ="; w(0)

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
  DIM sp() = INTERP.SPLREP(xs, ys)
  DIM xq(0) AS DOUBLE = {2.5}
  DIM sv() = INTERP.SPLEV(sp, xq)
  PRINT "splev     ="; sv(0)

  ' pdist/cdist/distance_matrix
  DIM X(2, 1) AS DOUBLE
  X = {{0, 0}, {3, 4}, {6, 8}}
  DIM Y(0, 1) AS DOUBLE
  Y = {{0, 0}}
  DIM pd() = SPATIAL.PDIST(X, "euclidean")
  PRINT "pdist0    ="; pd(0)
  DIM cdm = SPATIAL.CDIST(X, Y, "euclidean")
  PRINT "cdist10   ="; MATRIX.GETVALUE(cdm, 1, 0)
  DIM dm = SPATIAL.DISTANCEMATRIX(X, Y)
  PRINT "distmat10 ="; MATRIX.GETVALUE(dm, 1, 0)

  ' rfft/irfft: cos(2πi/8), ricostruzione — RFFT ritorna [re0, im0, re1, im1, …]
  DIM x2(7) AS DOUBLE
  FOR i = 0 TO 7 : x2(i) = COS(2 * 3.141592653589793 * i / 8) : NEXT
  DIM rf() = SIGNAL.RFFT(x2)
  PRINT "rfft10    ="; rf(2)
  DIM re(4) AS DOUBLE
  DIM im(4) AS DOUBLE
  FOR k = 0 TO 4
    re(k) = rf(2 * k) : im(k) = rf(2 * k + 1)
  NEXT
  DIM ir() = SIGNAL.IRFFT(re, im)
  PRINT "irfft0    ="; ir(0)

  ' ConvexHull quadrato
  DIM pts(4, 1) AS DOUBLE
  pts = {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0.5, 0.5}}
  DIM hull() = SPATIAL.CONVEXHULL(pts)
  PRINT "hull      ="; hull(0); hull(1); hull(2); hull(3)
END SUB
