' MiniTest: SIGNAL.WELCH — densita' spettrale (array interleaved [f0, psd0, …]).
' Dal 2026-11-23 è nel pacchetto FFI `signal` (non più builtin con handle di matrice).
' EXPECT: welch: ok
IMPORT SIGNAL
SUB Main()
  DIM x(7) AS DOUBLE = {1, 2, 3, 4, 5, 4, 3, 2}
  DIM w() = SIGNAL.WELCH(x, 4, 1.0)
  IF LEN(w) > 0 THEN PRINT "welch: ok" ELSE PRINT "welch: fail"
END SUB
