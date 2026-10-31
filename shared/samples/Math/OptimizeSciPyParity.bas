' scipy.optimize completato: radici scalari (brentq/brenth/ridder/toms748),
' minimizzazione scalare (brentmin/minimizescalar), minimize (powell/tnc/cobyla/slsqp),
' fsolve, least_squares, linprog, brute, basinhopping, dual_annealing.
' Oracoli: scipy 1.18.
IMPORT OPTIMIZE

SUB Main()
  ' --- radici scalari (return DOUBLE) ---
  r1 = OPTIMIZE.BRENTQ(FN(x AS DOUBLE) AS DOUBLE => x * x - 2, 0, 2)
  PRINT "brentq   ="; r1
  r2 = OPTIMIZE.BRENTH(FN(x AS DOUBLE) AS DOUBLE => x * x * x - 27, 2, 4)
  PRINT "brenth   ="; r2
  r3 = OPTIMIZE.RIDDER(FN(x AS DOUBLE) AS DOUBLE => x * x * x - 27, 2, 4)
  PRINT "ridder   ="; r3
  r4 = OPTIMIZE.TOMS748(FN(x AS DOUBLE) AS DOUBLE => SIN(x) - 0.5, 0, 1)
  PRINT "toms748  ="; r4

  ' --- minimizzazione scalare ---
  m1 = OPTIMIZE.BRENTMIN(FN(x AS DOUBLE) AS DOUBLE => (x - 3) * (x - 3), 0, 5)
  PRINT "brentmin ="; m1
  m2 = OPTIMIZE.MINIMIZESCALAR(FN(x AS DOUBLE) AS DOUBLE => (x + 1) * (x + 1) + 2, -5, 5, "brent")
  PRINT "minsc    ="; m2

  ' --- minimize multivariata (Rosenbrock → [1,1]) ---
  DIM x0(1) AS DOUBLE = {-1.2, 1}
  DIM pw() = OPTIMIZE.POWELL(FN(p() AS DOUBLE) AS DOUBLE => (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) * p(0)) ^ 2, x0)
  PRINT "powell0  ="; pw(0)
  PRINT "powell1  ="; pw(1)
  DIM bd(1) AS ARRAY OF DOUBLE
  bd(0) = {-5, 5} : bd(1) = {-5, 5}
  DIM tn() = OPTIMIZE.TNC(FN(p() AS DOUBLE) AS DOUBLE => (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) * p(0)) ^ 2, x0, bd)
  PRINT "tnc0     ="; tn(0)
  DIM sl() = OPTIMIZE.SLSQP(FN(p() AS DOUBLE) AS DOUBLE => (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) * p(0)) ^ 2, x0, bd)
  PRINT "slsqp0   ="; sl(0)
  DIM cb() = OPTIMIZE.COBYLA(FN(p() AS DOUBLE) AS DOUBLE => (p(0) + 2) * (p(0) + 2) + (p(1) + 1) * (p(1) + 1), x0, bd)
  PRINT "cobyla0  ="; cb(0)
  DIM mn() = OPTIMIZE.MINIMIZE(FN(p() AS DOUBLE) AS DOUBLE => (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) * p(0)) ^ 2, x0, "powell")
  PRINT "minim0   ="; mn(0)

  ' --- root multivariata (fsolve): x²+y²=1, x=y → [1/√2, 1/√2] ---
  DIM x02(1) AS DOUBLE = {0.5, 0.5}
  DIM fs() = OPTIMIZE.FSOLVE(FN(p() AS DOUBLE) AS ARRAY OF DOUBLE => {p(0) * p(0) + p(1) * p(1) - 1, p(0) - p(1)}, x02)
  PRINT "fsolve0  ="; fs(0)
  PRINT "fsolve1  ="; fs(1)

  ' --- least_squares: fit y = a*x+b su (1,2),(2,3),(3,5) → [1.5, 1/3] ---
  DIM x03(1) AS DOUBLE = {0, 0}
  DIM ls() = OPTIMIZE.LEASTSQUARESFIT(FN(p() AS DOUBLE) AS ARRAY OF DOUBLE => {p(0) * 1 + p(1) - 2, p(0) * 2 + p(1) - 3, p(0) * 3 + p(1) - 5}, x03)
  PRINT "lstsq0   ="; ls(0)
  PRINT "lstsq1   ="; ls(1)

  ' --- linprog: min -x-y, x+2y≤4, 3x+2y≤6, x,y≥0 → [1, 1.5] ---
  DIM Aub(1) AS ARRAY OF DOUBLE
  Aub(0) = {1, 2} : Aub(1) = {3, 2}
  DIM bub(1) AS DOUBLE = {4, 6}
  DIM lp() = OPTIMIZE.LINPROG({-1, -1}, Aub, bub)
  PRINT "linprog0 ="; lp(0)
  PRINT "linprog1 ="; lp(1)

  ' --- globali ---
  DIM rg(1) AS ARRAY OF DOUBLE
  rg(0) = {-3, 3} : rg(1) = {-3, 3}
  DIM br() = OPTIMIZE.BRUTE(FN(p() AS DOUBLE) AS DOUBLE => (p(0) - 1) * (p(0) - 1) + (p(1) + 2) * (p(1) + 2), rg, 41)
  PRINT "brute0   ="; br(0)
  DIM bh() = OPTIMIZE.BASINHOPPING(FN(p() AS DOUBLE) AS DOUBLE => (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) * p(0)) ^ 2, x0, 30)
  PRINT "basin0   ="; bh(0)
  DIM bg(1) AS ARRAY OF DOUBLE
  bg(0) = {-10, 10} : bg(1) = {-10, 10}
  DIM da() = OPTIMIZE.DUALANNEALING(FN(p() AS DOUBLE) AS DOUBLE => (p(0) - 1) * (p(0) - 1) + (p(1) + 2) * (p(1) + 2), bg, 2000)
  PRINT "dual0    ="; da(0)
  PRINT "dual1    ="; da(1)
END SUB
