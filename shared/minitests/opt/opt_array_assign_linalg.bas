' EXPECT: lstsq= 1.5   0.3333|chol= 1.75   1.5|linprog= 1   1.5|simplex= 0   0|lsfit= 1.5   0.3333
' Builtin OPTIMIZE lineari assegnati a un array DICHIARATO (2026-11-03: su C99 la
' forma `a = OPTIMIZE.X(...)` dava "simbolo non trovato: funzione o subroutine".
IMPORT OPTIMIZE
IMPORT MATH
SUB Main()
  DIM A(2) AS ARRAY OF DOUBLE
  A(0) = {1.0, 1.0} : A(1) = {2.0, 1.0} : A(2) = {3.0, 1.0}
  DIM b(2) AS DOUBLE = {2.0, 3.0, 5.0}
  DIM ls(1) AS DOUBLE
  ls = OPTIMIZE.LEASTSQUARES(A, b)
  PRINT "lstsq="; MATH.ROUND(ls(0), 4); " "; MATH.ROUND(ls(1), 4)
  DIM C(1) AS ARRAY OF DOUBLE
  C(0) = {4.0, 2.0} : C(1) = {2.0, 3.0}
  DIM d(1) AS DOUBLE = {10.0, 8.0}
  DIM ch(1) AS DOUBLE
  ch = OPTIMIZE.CHOLESKYSOLVE(C, d)
  PRINT "chol="; MATH.ROUND(ch(0), 4); " "; MATH.ROUND(ch(1), 4)
  DIM Aub(1) AS ARRAY OF DOUBLE
  Aub(0) = {1.0, 2.0} : Aub(1) = {3.0, 2.0}
  DIM bub(1) AS DOUBLE = {4.0, 6.0}
  DIM lp(1) AS DOUBLE
  lp = OPTIMIZE.LINPROG({-1.0, -1.0}, Aub, bub)
  PRINT "linprog="; MATH.ROUND(lp(0), 4); " "; MATH.ROUND(lp(1), 4)
  DIM sp(1) AS DOUBLE
  sp = OPTIMIZE.SIMPLEX({-1.0, -1.0}, Aub, bub)
  PRINT "simplex="; MATH.ROUND(sp(0), 4); " "; MATH.ROUND(sp(1), 4)
  DIM x0(1) AS DOUBLE = {0.0, 0.0}
  DIM lf(1) AS DOUBLE
  lf = OPTIMIZE.LEASTSQUARESFIT(FN(p() AS DOUBLE) AS ARRAY OF DOUBLE => {p(0) * 1 + p(1) - 2, p(0) * 2 + p(1) - 3, p(0) * 3 + p(1) - 5}, x0)
  PRINT "lsfit="; MATH.ROUND(lf(0), 4); " "; MATH.ROUND(lf(1), 4)
END SUB
