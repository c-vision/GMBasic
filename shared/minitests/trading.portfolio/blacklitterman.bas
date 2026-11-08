' MiniTest: PORTFOLIO.BLACKLITTERMAN — parita' 4 backend (LEN weights).
' EXPECT: 3
IMPORT PORTFOLIO AS TP
SUB Main
  DIM caps(2) AS DOUBLE : caps = {100.0, 200.0, 300.0}
  DIM cov(2) AS ARRAY OF DOUBLE
  cov(0) = {0.04, 0.005, 0.0} : cov(1) = {0.005, 0.01, 0.0} : cov(2) = {0.0, 0.0, 0.0025}
  DIM P(2) AS ARRAY OF DOUBLE
  P(0) = {1.0, 0.0, 0.0} : P(1) = {0.0, 1.0, 0.0} : P(2) = {0.0, 0.0, 1.0}
  DIM Q(2) AS DOUBLE : Q = {0.10, 0.05, 0.02}
  DIM omega(2) AS ARRAY OF DOUBLE
  omega(0) = {0.0001, 0.0, 0.0} : omega(1) = {0.0, 0.0001, 0.0} : omega(2) = {0.0, 0.0, 0.0001}
  DIM x() AS DOUBLE
  x = TP.BLACKLITTERMAN(caps, cov, 2.5, 0.05, P, Q, omega, 0.01)
  PRINT UBOUND(x) - 2
END SUB
