' EXPECT: splev= 12.25
IMPORT ANALYSIS
SUB Main()
  DIM xs(4) AS DOUBLE = {0, 1, 2, 3, 4}
  DIM ys(4) AS DOUBLE = {1, 4, 9, 16, 25}
  DIM sp = ANALYSIS.Splrep(xs, ys)
  DIM xq(0) AS DOUBLE = {2.5}
  DIM sv() = ANALYSIS.Splev(sp, xq)
  PRINT "splev="; ROUND(sv(0), 6)
END SUB