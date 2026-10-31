' OPTIMIZE.MINIMIZE nella forma a 3 argomenti (f, x0, metodo) SENZA bounds.
' BUG (C99Native): `dim_arr_optimize` leggeva args[3] (bounds) fuori range → SEGFAULT del
' driver su OptimizeSciPyParity; il runtime tratta bounds=NULL come illimitato.
' EXPECT: min3= 1   1
IMPORT OPTIMIZE
IMPORT MATH
SUB Main()
  DIM x0(1) AS DOUBLE = {-1.2, 1.0}
  DIM f = FN(p() AS DOUBLE) AS DOUBLE => (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) * p(0)) ^ 2
  DIM mn() = OPTIMIZE.MINIMIZE(f, x0, "powell")
  PRINT "min3="; MATH.ROUND(mn(0), 3); " "; MATH.ROUND(mn(1), 3)
END SUB
