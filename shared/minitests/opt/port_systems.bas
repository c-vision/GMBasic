' EXPECT: BR= 1.414214 , 3|FS= 1.414214 , 3
' Port FFI di optimize/systems.bas.
IMPORT OPTIMIZE
IMPORT MATH
SUB Main()
  DIM Sys = FN(v() AS DOUBLE) AS ARRAY OF DOUBLE => {v(0) ^ 2 - 2, v(1) - 3}
  DIM Guess(1) AS DOUBLE = {1.0, 0.0}
  DIM o(1) AS DOUBLE
  CALL OPTIMIZE.BROYDEN(Sys, Guess, o)
  PRINT "BR="; MATH.ROUND(o(0), 6); ","; MATH.ROUND(o(1), 6)
  CALL OPTIMIZE.FSOLVE(Sys, Guess, o)
  PRINT "FS="; MATH.ROUND(o(0), 6); ","; MATH.ROUND(o(1), 6)
END SUB
