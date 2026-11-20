' MiniTest ANALYSIS — INTERPOLATELINEAR fuori range estrapola, INTERPOLATESTEP clampa.
' Il kernel C clampava l'interpolazione lineare; ora estrapola come MathNet (le 4 gambe
' concordano) e il clamping resta la funzione separata INTERPOLATECLAMP/InterpolateClamp.
' y = 2x su x = {0,1,2}: fuori range 3 → 6 e -1 → -2; lo step invece tiene i bordi (4 e 0).
' EXPECT: extrap= 6 |-2|step= 4 | 0
IMPORT TIMESERIES

SUB Main()
  DIM x() = {0, 1, 2}
  DIM y() = {0, 2, 4}
  PRINT "extrap="; NUMERICAL.InterpolateLinear(x, y, 3); "|"; NUMERICAL.InterpolateLinear(x, y, -1)
  PRINT "step="; NUMERICAL.InterpolateStep(x, y, 3); "|"; NUMERICAL.InterpolateStep(x, y, -1)
END SUB
