' EXPECT: newton= 2|brent= 1.99999999999998|bisection= 1.99999999999989|secant= 1.99999999999995|brentq= 1.99999999999998|brenth= 1.99999999999998|ridder= 2|toms748= 2|goldsect= 1.99999999999519|brentmin= 2|minscalar= 2
' Port FFI di optimize/scalar.bas (stesso EXPECT del builtin).
IMPORT OPT
SUB Main()
  DIM f = FN(x AS DOUBLE) AS DOUBLE => x ^ 2 - 4
  DIM g = FN(x AS DOUBLE) AS DOUBLE => (x - 2) ^ 2
  PRINT "newton="; OPT.NEWTON(f, 1.0)
  PRINT "brent="; OPT.BRENT(f, 0.0, 5.0)
  PRINT "bisection="; OPT.BISECTION(f, 0.0, 5.0)
  PRINT "secant="; OPT.SECANT(f, 1.0, 5.0)
  PRINT "brentq="; OPT.BRENTQ(f, 0.0, 5.0)
  PRINT "brenth="; OPT.BRENTH(f, 0.0, 5.0)
  PRINT "ridder="; OPT.RIDDER(f, 0.0, 5.0)
  PRINT "toms748="; OPT.TOMS748(f, 0.0, 5.0)
  PRINT "goldsect="; OPT.GOLDENSECTION(g, 0.0, 5.0)
  PRINT "brentmin="; OPT.BRENTMIN(g, 0.0, 5.0)
  PRINT "minscalar="; OPT.MINIMIZESCALAR(g, 0.0, 5.0)
END SUB
