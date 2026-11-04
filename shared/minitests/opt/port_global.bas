' EXPECT: B= 3
' Port FFI di optimize/global.bas.
IMPORT OPTIMIZE
SUB Main()
  DIM f = FN(x() AS DOUBLE) AS DOUBLE => (x(0) - 3) ^ 2 + 1
  DIM X0(0) AS DOUBLE = {10.0}
  DIM o(0) AS DOUBLE
  CALL OPTIMIZE.BASINHOPPINGN(f, X0, 5, o)
  PRINT "B="; MATH.ROUND(o(0), 4)
END SUB
