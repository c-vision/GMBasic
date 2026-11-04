' EXPECT: min3= 1   1
' Port FFI di optimize/minimize_nobounds.bas.
IMPORT OPTIMIZE
IMPORT MATH
SUB Main()
  DIM x0(1) AS DOUBLE = {-1.2, 1.0}
  DIM f = FN(p() AS DOUBLE) AS DOUBLE => (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) * p(0)) ^ 2
  DIM o(1) AS DOUBLE
  CALL OPTIMIZE.MINIMIZEM(f, x0, "powell", o)
  PRINT "min3="; MATH.ROUND(o(0), 3); " "; MATH.ROUND(o(1), 3)
END SUB
