' EXPECT: lstsq= 1.5   0.3333|chol= 1.75   1.5|linprog= 1   1.5|simplex= 0   0|lsfit= 1.5   0.3333
' Port FFI di optimize/linalg.bas.
IMPORT OPTIMIZE
SUB Main()
  ' stessa matrice del sample: A righe (1,1) (2,1) (3,1) e b = (2,3,5)
  DIM Af(2, 1) AS DOUBLE
  Af = {{1.0, 1.0}, {2.0, 1.0}, {3.0, 1.0}}
  DIM b(2) AS DOUBLE = {2.0, 3.0, 5.0}
  DIM o(1) AS DOUBLE
  CALL OPTIMIZE.LEASTSQUARES(Af, b, o)
  PRINT "lstsq="; MATH.ROUND(o(0), 4); " "; MATH.ROUND(o(1), 4)
  DIM Cf(1, 1) AS DOUBLE
  Cf = {{4.0, 2.0}, {2.0, 3.0}}
  DIM bc(1) AS DOUBLE = {10.0, 8.0}
  CALL OPTIMIZE.CHOLESKYSOLVE(Cf, bc, o)
  PRINT "chol="; MATH.ROUND(o(0), 4); " "; MATH.ROUND(o(1), 4)
  DIM Aubf(1, 1) AS DOUBLE
  Aubf = {{1.0, 2.0}, {3.0, 2.0}}
  DIM bub(1) AS DOUBLE = {4.0, 6.0}
  DIM cneg(1) AS DOUBLE = {-1.0, -1.0}
  CALL OPTIMIZE.LINPROG(cneg, Aubf, bub, o)
  PRINT "linprog="; MATH.ROUND(o(0), 4); " "; MATH.ROUND(o(1), 4)
  DIM sx(2) AS DOUBLE
  CALL OPTIMIZE.SIMPLEX(cneg, Aubf, bub, sx)
  PRINT "simplex="; MATH.ROUND(sx(0), 4); " "; MATH.ROUND(sx(1), 4)
  DIM x0(1) AS DOUBLE = {0.0, 0.0}
  DIM r5(4) AS DOUBLE
  CALL OPTIMIZE.LEASTSQUARESFIT(FN(p() AS DOUBLE) AS ARRAY OF DOUBLE => {p(0) * 1 + p(1) - 2, p(0) * 2 + p(1) - 3, p(0) * 3 + p(1) - 5}, x0, r5)
  PRINT "lsfit="; MATH.ROUND(r5(0), 4); " "; MATH.ROUND(r5(1), 4)
END SUB
