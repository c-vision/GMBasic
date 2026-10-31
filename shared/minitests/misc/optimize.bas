' OPTIMIZE scalari su C99Native (callback lambda → gmb_fn_t, solver gmb_opt_*_c99).
IMPORT OPTIMIZE
SUB Main()
  DIM Radice = OPTIMIZE.NEWTON({ |x| RETURN x ^ 2 - 4 }, 1.0)
  PRINT "newton="; Radice
  DIM R2 = OPTIMIZE.BISECTION({ |x| RETURN x ^ 2 - 4 }, 0.0, 5.0)
  PRINT "bisection="; R2
  DIM R3 = OPTIMIZE.SECANT({ |x| RETURN x ^ 2 - 4 }, 1.0, 5.0)
  PRINT "secant="; R3
END SUB
