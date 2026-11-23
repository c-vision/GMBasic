' MiniTest ANALYSIS.SPLREP via FFI (namespace INTERP del pacchetto `math`).
' Array combinato: 1 + n + 4(n-1) = 5n-3 = 22 elementi per n=5.
' EXPECT: splrep= 22
IMPORT INTERP
SUB Main()
  DIM xs(4) AS DOUBLE = {0, 1, 2, 3, 4}
  DIM ys(4) AS DOUBLE = {1, 4, 9, 16, 25}
  DIM sp() = INTERP.SPLREP(xs, ys)
  PRINT "splrep="; LEN(sp)
END SUB
