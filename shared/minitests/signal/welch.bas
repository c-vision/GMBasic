' MiniTest: SIGNAL.WELCH — densita' spettrale (handle matrice, parita' 4 backend).
' EXPECT: welch: ok
IMPORT SIGNAL
SUB Main()
  DIM x(7) AS DOUBLE = {1, 2, 3, 4, 5, 4, 3, 2}
  DIM h = SIGNAL.WELCH(x, 4, 1.0)
  IF h > 0 THEN PRINT "welch: ok" ELSE PRINT "welch: fail"
END SUB
