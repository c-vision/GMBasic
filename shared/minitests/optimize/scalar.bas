' EXPECT: newton= 2|brent= 2.00000000162127|bisection= 2.00000000186265|secant= 1.99999999999995|brentq= 1.99999999999998|brenth= 1.99999999999998|ridder= 2|toms748= 2|goldsect= 2.00000000059108|brentmin= 2|minscalar= 2
IMPORT OPTIMIZE
SUB Main()
  PRINT "newton="; OPTIMIZE.NEWTON({ |x| RETURN x ^ 2 - 4 }, 1.0)
  PRINT "brent="; OPTIMIZE.BRENT({ |x| RETURN x ^ 2 - 4 }, 0.0, 5.0)
  PRINT "bisection="; OPTIMIZE.BISECTION({ |x| RETURN x ^ 2 - 4 }, 0.0, 5.0)
  PRINT "secant="; OPTIMIZE.SECANT({ |x| RETURN x ^ 2 - 4 }, 1.0, 5.0)
  PRINT "brentq="; OPTIMIZE.BRENTQ({ |x| RETURN x ^ 2 - 4 }, 0.0, 5.0)
  PRINT "brenth="; OPTIMIZE.BRENTH({ |x| RETURN x ^ 2 - 4 }, 0.0, 5.0)
  PRINT "ridder="; OPTIMIZE.RIDDER({ |x| RETURN x ^ 2 - 4 }, 0.0, 5.0)
  PRINT "toms748="; OPTIMIZE.TOMS748({ |x| RETURN x ^ 2 - 4 }, 0.0, 5.0)
  PRINT "goldsect="; OPTIMIZE.GOLDENSECTION({ |x| RETURN (x - 2) ^ 2 }, 0.0, 5.0)
  PRINT "brentmin="; OPTIMIZE.BRENTMIN({ |x| RETURN (x - 2) ^ 2 }, 0.0, 5.0)
  PRINT "minscalar="; OPTIMIZE.MINIMIZESCALAR({ |x| RETURN (x - 2) ^ 2 }, 0.0, 5.0)
END SUB
