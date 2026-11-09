' MiniTest: PORTFOLIO.MINCVAR — parita' 4 backend.
' EXPECT: 1
IMPORT PORTFOLIO AS TP

SUB Main
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2, 2) AS DOUBLE
  cov = {{0.04, 0.005, 0.0}, {0.005, 0.01, 0.0}, {0.0, 0.0, 0.0025}}
  DIM hist(2, 4) AS DOUBLE
  hist = {{0.02, -0.01, 0.03, 0.01, -0.02}, {0.01, 0.005, -0.005, 0.02, 0.01}, {0.005, 0.01, -0.01, 0.015, 0.005}}
  DIM x() AS DOUBLE
  x = TP.MINCVAR(hist, 0.95, 0.01)
  PRINT UBOUND(x) - 2
END SUB
