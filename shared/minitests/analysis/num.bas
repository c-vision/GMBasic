' EXPECT: 9.5|2.25|6.5|4|6.25|6.21875|9|6.5|40
SUB Main()
  DIM x(3) AS DOUBLE = {0, 1, 2, 3}
  DIM y(3) AS DOUBLE = {0, 1, 4, 9}
  PRINT NUMERICAL.Trapezoid(y, x)
  PRINT NUMERICAL.CubicSpline(x, y, 1.5)
  PRINT NUMERICAL.InterpolateLinear(x, y, 2.5)
  PRINT NUMERICAL.InterpolateStep(x, y, 2.5)
  PRINT NUMERICAL.InterpolatePolynomial(x, y, 2.5)
  PRINT NUMERICAL.InterpolatePchip(x, y, 2.5)
  PRINT NUMERICAL.InterpolateClamp(x, y, 5)
  PRINT NUMERICAL.InterpolateGeneric("linear", x, y, 2.5)
  PRINT VECTOR.EuclideanDistanceSquared(x, y)
END SUB
