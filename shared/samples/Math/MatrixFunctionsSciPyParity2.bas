' scipy.linalg (2° lotto): cosm/sinm/tanm/signm/fractional_power,
' orth/subspace_angles/polar/cond/nuclear_norm, toeplitz/hankel/circulant,
' solve_triangular, eig (generale, non simmetrica) e schur. Tutto via LINALG FFI.
' Oracoli: scipy 1.18.
IMPORT LINALG
SUB Main()
  DIM a(3) AS DOUBLE
  a(0)=2 : a(1)=1 : a(2)=1 : a(3)=2

  DIM c(3) AS DOUBLE
  LINALG.COSM(a, 2, c)
  PRINT "cosm00   ="; c(0)
  DIM s(3) AS DOUBLE
  LINALG.SINM(a, 2, s)
  PRINT "sinm01   ="; s(1)
  DIM tn(3) AS DOUBLE
  LINALG.TANM(a, 2, tn)
  PRINT "tanm00   ="; tn(0)
  DIM sg(3) AS DOUBLE
  LINALG.SIGNM(a, 2, sg)
  PRINT "signm01  ="; sg(1)
  DIM fp(3) AS DOUBLE
  LINALG.FRACTIONALPOWER(a, 2, 0.5, fp)
  PRINT "fp00     ="; fp(0)

  ' g = [[1,2],[2,3]] (definita positiva, autovalori reali)
  DIM g(3) AS DOUBLE
  g(0)=1 : g(1)=2 : g(2)=2 : g(3)=3

  DIM o(3) AS DOUBLE
  LINALG.ORTH(g, 2, 2, o)
  PRINT "orth00   ="; o(0)
  DIM ang(1) AS DOUBLE
  DIM nang AS LONG
  nang = LINALG.SUBSPACEANGLES(a, 2, 2, g, 2, 2, ang)
  PRINT "anglen   ="; nang
  PRINT "angle0   ="; ang(0)
  DIM pu(3) AS DOUBLE, pp(3) AS DOUBLE
  LINALG.POLAR(g, 2, pu, pp)
  PRINT "polar00  ="; pu(0)
  PRINT "polarP00 ="; pp(0)
  PRINT "cond     ="; LINALG.COND(g, 2, 2)
  PRINT "nuc      ="; LINALG.NUCLEARNORM(g, 2, 2)

  ' costruttori MATRIX
  DIM c1(2), r1(2)
  c1(0) = 1 : c1(1) = 2 : c1(2) = 3
  r1(0) = 1 : r1(1) = 4 : r1(2) = 5
  DIM tp = MATRIX.TOEPLITZ(c1, r1)
  PRINT "toep01   ="; MATRIX.GETVALUE(tp, 0, 1)
  PRINT "toep10   ="; MATRIX.GETVALUE(tp, 1, 0)
  DIM hk = MATRIX.HANKEL(c1, r1)
  PRINT "hankel21 ="; MATRIX.GETVALUE(hk, 2, 1)
  DIM cc = MATRIX.CIRCULANT(c1)
  PRINT "circ01   ="; MATRIX.GETVALUE(cc, 0, 1)

  ' solve_triangular: L = [[2,0],[1,2]], b = [4,5] -> x = [2,1.5]
  DIM L(3) AS DOUBLE
  L(0)=2 : L(1)=0 : L(2)=1 : L(3)=2
  DIM b(1) AS DOUBLE
  b(0) = 4 : b(1) = 5
  DIM x(1) AS DOUBLE
  LINALG.SOLVETRIANGULAR(L, 2, b, 1, x)
  PRINT "st00     ="; x(0)

  ' eig generale: A = [[2,1],[1,2]] -> autovalori 3, 1
  DIM wr(1) AS DOUBLE, wi(1) AS DOUBLE, vr(3) AS DOUBLE, vi(3) AS DOUBLE
  LINALG.EIG(a, 2, wr, wi, vr, vi)
  PRINT "eigreal0 ="; wr(0)
  PRINT "eigreal1 ="; wr(1)
  PRINT "eigvr00  ="; vr(0)
  ' schur: A = Z T Z^T
  DIM t(3) AS DOUBLE, z(3) AS DOUBLE
  LINALG.SCHUR(a, 2, t, z)
  PRINT "schur00  ="; t(0)
END SUB
