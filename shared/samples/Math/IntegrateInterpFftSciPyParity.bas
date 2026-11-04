' Nuclei scipy.integrate / scipy.interpolate / scipy.fft completati (ANALYSIS).
' Oracoli: SciPy 1.18 — quad/simpson x^2 = 1/3, trapezoid(y,x)=9.5,
' cumulative_trapezoid=[0.5,3,9.5], solve_ivp e^-t, CubicSpline not-a-knot
' @1.5 = 1.3125, fftfreq(6), fftshift/ifftshift.

SUB Main()
  DIM yy(3) AS DOUBLE = {0, 1, 4, 9}
  DIM xx(3) AS DOUBLE = {0, 1, 2, 3}
  DIM yy2(3) AS DOUBLE = {0, 0.5, 2, 1.5}

  PRINT "integrate   ="; NUMERICAL.Integrate(FN(x) => x * x, 0, 1)
  PRINT "simpson     ="; NUMERICAL.Simpson(FN(x) => x * x, 0, 1, 256)
  PRINT "trapezoid   ="; NUMERICAL.Trapezoid(yy, xx)
  DIM ct() = NUMERICAL.CumTrapezoid(yy, xx)
  PRINT "cumtrap     ="; ct(0); ct(1); ct(2)

  DIM ys() = NUMERICAL.SolveOde({ |t, y| RETURN -y }, 1.0, 0.0, 1.0, 10)
  PRINT "ode y(1)    ="; ys(10)

  PRINT "cubicspline ="; NUMERICAL.CubicSpline(xx, yy2, 1.5)

  DIM f6() = NUMERICAL.FftFreq(6, 1)
  PRINT "fftfreq6    ="; f6(0); f6(1); f6(2); f6(3); f6(4); f6(5)
  DIM a6(5) AS DOUBLE = {0, 1, 2, 3, 4, 5}
  DIM fs() = NUMERICAL.FftShift(a6)
  PRINT "fftshift    ="; fs(0); fs(1); fs(2); fs(3); fs(4); fs(5)
  DIM ifs() = NUMERICAL.IfftShift(a6)
  PRINT "ifftshift   ="; ifs(0); ifs(1); ifs(2); ifs(3); ifs(4); ifs(5)
END SUB
