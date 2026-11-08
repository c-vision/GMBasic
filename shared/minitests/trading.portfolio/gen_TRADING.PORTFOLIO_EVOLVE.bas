' NOTE PCode: PCode EVOLVE handler mismatch (popolazione/fitness closure vs funzione reale).
' IL/VMC concordano su 3; PCode torna 0 per bug handler (non modificabile senza toccare C#).
' EXPECT: 3
IMPORT PORTFOLIO AS TP

SUB Main
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2) AS ARRAY OF DOUBLE
  cov(0) = {0.04, 0.005, 0.0} : cov(1) = {0.005, 0.01, 0.0} : cov(2) = {0.0, 0.0, 0.0025}
  DIM x() AS DOUBLE
  x = TP.EVOLVE(r, cov)
  PRINT UBOUND(x) - 2
END SUB
