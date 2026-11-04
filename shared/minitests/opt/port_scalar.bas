' EXPECT: newton= 2|brent= 1.99999999999998|bisection= 1.99999999999989|secant= 1.99999999999995|brentq= 1.99999999999998|brenth= 1.99999999999998|ridder= 2|toms748= 2|goldsect= 1.99999999999519|brentmin= 2|minscalar= 2
' Port FFI di optimize/scalar.bas (stesso EXPECT del builtin).
IMPORT OPTIMIZE
SUB Main()
  DIM f = FN(x AS DOUBLE) AS DOUBLE => x ^ 2 - 4
  DIM g = FN(x AS DOUBLE) AS DOUBLE => (x - 2) ^ 2
  PRINT "newton="; OPTIMIZE.NEWTON(f, 1.0)
  PRINT "brent="; OPTIMIZE.BRENT(f, 0.0, 5.0)
  PRINT "bisection="; OPTIMIZE.BISECTION(f, 0.0, 5.0)
  PRINT "secant="; OPTIMIZE.SECANT(f, 1.0, 5.0)
  PRINT "brentq="; OPTIMIZE.BRENTQ(f, 0.0, 5.0)
  PRINT "brenth="; OPTIMIZE.BRENTH(f, 0.0, 5.0)
  PRINT "ridder="; OPTIMIZE.RIDDER(f, 0.0, 5.0)
  PRINT "toms748="; OPTIMIZE.TOMS748(f, 0.0, 5.0)
  PRINT "goldsect="; OPTIMIZE.GOLDENSECTION(g, 0.0, 5.0)
  PRINT "brentmin="; OPTIMIZE.BRENTMIN(g, 0.0, 5.0)
  PRINT "minscalar="; OPTIMIZE.MINIMIZESCALAR(g, 0.0, 5.0)
END SUB
