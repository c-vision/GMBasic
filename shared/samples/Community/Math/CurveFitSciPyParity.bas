' scipy.optimize.curve_fit: fit non lineare ai minimi quadrati.
' Modello y = a*exp(b*x) + c, dati = oracle scipy (popt=[2.5162, 0.7973, 0.9859]).
IMPORT OPTIMIZE

SUB Main()
  DIM x(8) AS DOUBLE = {0, 0.25, 0.5, 0.75, 1, 1.25, 1.5, 1.75, 2}
  DIM y(8) AS DOUBLE = {3.504967, 4.052124, 4.736039, 5.570527, 6.561511, 7.793363, 9.316084, 11.145674, 13.377886}
  DIM p0(2) AS DOUBLE = {1, 1, 1}
  DIM Fit() = OPTIMIZE.CURVEFIT(FN(p() AS DOUBLE, xx AS DOUBLE) AS DOUBLE => p(0) * EXP(p(1) * xx) + p(2), x, y, p0)
  PRINT "curvefit  ="; Fit(0); Fit(1); Fit(2)
END SUB
