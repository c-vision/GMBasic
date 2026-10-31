' MiniTest: TRADING.PORTFOLIO.EXPECTEDRETURN — parita' 4 backend.
' EXPECT: 0.069
IMPORT TRADING.PORTFOLIO AS TP

SUB Main
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2) AS ARRAY OF DOUBLE
  cov(0) = {0.04, 0.005, 0.0} : cov(1) = {0.005, 0.01, 0.0} : cov(2) = {0.0, 0.0, 0.0025}
  DIM hist(2) AS ARRAY OF DOUBLE
  hist(0) = {0.02, -0.01, 0.03, 0.01, -0.02} : hist(1) = {0.01, 0.005, -0.005, 0.02, 0.01} : hist(2) = {0.005, 0.01, -0.01, 0.015, 0.005}
  PRINT ROUND(TP.EXPECTEDRETURN(w, r), 8)
END SUB
