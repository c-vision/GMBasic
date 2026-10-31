' scipy.optimize.differential_evolution: ottimizzazione globale (DE/rand/1/bin).
' Rosenbrock n=3, minimo globale [1,1,1] → f=0. Oracolo: scipy 1.18.
IMPORT OPTIMIZE

SUB Main()
  DIM Rf = FN(v() AS DOUBLE) AS DOUBLE => 100 * (v(1) - v(0) * v(0)) ^ 2 + (1 - v(0)) ^ 2 + 100 * (v(2) - v(1) * v(1)) ^ 2 + (1 - v(1)) ^ 2
  DIM Bnd(2) AS ARRAY OF DOUBLE
  Bnd(0) = {-5, 5} : Bnd(1) = {-5, 5} : Bnd(2) = {-5, 5}
  DIM Best() = OPTIMIZE.DIFFERENTIALEVOLUTION(Rf, Bnd)
  PRINT "de        ="; Best(0); Best(1); Best(2)
END SUB
