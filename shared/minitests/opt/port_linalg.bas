' EXPECT: lstsq= 1.5   0.3333|chol= 1.75   1.5|linprog= 1   1.5|simplex= 0   0|lsfit= 1.5   0.3333
' Port FFI di optimize/linalg.bas.
IMPORT OPT
IMPORT MATH
SUB Main()
  ' stessa matrice del sample: A righe (1,1) (2,1) (3,1) e b = (2,3,5)
  DIM Aj(2) AS ARRAY OF DOUBLE
  Aj(0) = {1.0, 1.0} : Aj(1) = {2.0, 1.0} : Aj(2) = {3.0, 1.0}
  DIM Af(5) AS DOUBLE
  Af(0) = 1.0 : Af(1) = 1.0 : Af(2) = 2.0 : Af(3) = 1.0 : Af(4) = 3.0 : Af(5) = 1.0
  DIM b(2) AS DOUBLE = {2.0, 3.0, 5.0}
  DIM o(1) AS DOUBLE
  CALL OPT.LEASTSQUARES(Af, 3, 2, b, o)
  PRINT "lstsq="; MATH.ROUND(o(0), 4); " "; MATH.ROUND(o(1), 4)
  DIM Cj(1) AS ARRAY OF DOUBLE
  Cj(0) = {4.0, 2.0} : Cj(1) = {2.0, 3.0}
  DIM Cf(3) AS DOUBLE
  Cf(0) = 4.0 : Cf(1) = 2.0 : Cf(2) = 2.0 : Cf(3) = 3.0
  DIM bc(1) AS DOUBLE = {10.0, 8.0}
  CALL OPT.CHOLESKYSOLVE(Cf, 2, bc, o)
  PRINT "chol="; MATH.ROUND(o(0), 4); " "; MATH.ROUND(o(1), 4)
  DIM Aub(1) AS ARRAY OF DOUBLE
  Aub(0) = {1.0, 2.0} : Aub(1) = {3.0, 2.0}
  DIM Aubf(3) AS DOUBLE
  Aubf(0) = 1.0 : Aubf(1) = 2.0 : Aubf(2) = 3.0 : Aubf(3) = 2.0
  DIM bub(1) AS DOUBLE = {4.0, 6.0}
  DIM cneg(1) AS DOUBLE = {-1.0, -1.0}
  CALL OPT.LINPROG(cneg, 2, Aubf, 2, bub, o)
  PRINT "linprog="; MATH.ROUND(o(0), 4); " "; MATH.ROUND(o(1), 4)
  DIM sx(2) AS DOUBLE
  CALL OPT.SIMPLEX(cneg, 2, Aubf, 2, bub, sx)
  PRINT "simplex="; MATH.ROUND(sx(0), 4); " "; MATH.ROUND(sx(1), 4)
  DIM x0(1) AS DOUBLE = {0.0, 0.0}
  DIM r5(4) AS DOUBLE
  CALL OPT.LEASTSQUARESFIT(FN(p() AS DOUBLE) AS ARRAY OF DOUBLE => {p(0) * 1 + p(1) - 2, p(0) * 2 + p(1) - 3, p(0) * 3 + p(1) - 5}, x0, r5)
  PRINT "lsfit="; MATH.ROUND(r5(0), 4); " "; MATH.ROUND(r5(1), 4)
END SUB
