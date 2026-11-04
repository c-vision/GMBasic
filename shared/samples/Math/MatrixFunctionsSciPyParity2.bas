' scipy.linalg completato (2° lotto): cosm/sinm/tanm/signm/fractional_power,
' orth/subspace_angles/polar/cond/nuclear_norm, toeplitz/hankel/circulant,
' solve_triangular, eig (generale, non simmetrica) e schur.
' Oracoli: scipy 1.18.

SUB Main()
  h = MATRIX.CREATE(2, 2)
  h = MATRIX.SETVALUE(h, 0, 0, 2) : h = MATRIX.SETVALUE(h, 0, 1, 1)
  h = MATRIX.SETVALUE(h, 1, 0, 1) : h = MATRIX.SETVALUE(h, 1, 1, 2)

  c = LINALG.COSM(h)
  PRINT "cosm00   ="; MATRIX.GETVALUE(c, 0, 0)
  s = LINALG.SINM(h)
  PRINT "sinm01   ="; MATRIX.GETVALUE(s, 0, 1)
  tn = LINALG.TANM(h)
  PRINT "tanm00   ="; MATRIX.GETVALUE(tn, 0, 0)
  sg = LINALG.SIGNM(h)
  PRINT "signm01  ="; MATRIX.GETVALUE(sg, 0, 1)
  fp = LINALG.FRACTIONALPOWER(h, 0.5)
  PRINT "fp00     ="; MATRIX.GETVALUE(fp, 0, 0)

  ' A = [[1,2],[2,3]] (definita positiva, autovalori reali)
  g = MATRIX.CREATE(2, 2)
  g = MATRIX.SETVALUE(g, 0, 0, 1) : g = MATRIX.SETVALUE(g, 0, 1, 2)
  g = MATRIX.SETVALUE(g, 1, 0, 2) : g = MATRIX.SETVALUE(g, 1, 1, 3)

  o = LINALG.ORTH(g)
  PRINT "orth00   ="; MATRIX.GETVALUE(o, 0, 0)
  DIM ang() = LINALG.SUBSPACEANGLES(h, g)
  PRINT "anglen   ="; UBOUND(ang) + 1
  PRINT "angle0   ="; ang(0)
  pol = LINALG.POLAR(g)
  pu = LINALG.FACTOR(pol, "U")
  pp = LINALG.FACTOR(pol, "P")
  PRINT "polar00  ="; MATRIX.GETVALUE(pu, 0, 0)
  PRINT "polarP00 ="; MATRIX.GETVALUE(pp, 0, 0)
  PRINT "cond     ="; LINALG.COND(g)
  PRINT "nuc      ="; LINALG.NUCLEARNORM(g)

  ' costruttori
  DIM c1(2), r1(2)
  c1(0) = 1 : c1(1) = 2 : c1(2) = 3
  r1(0) = 1 : r1(1) = 4 : r1(2) = 5
  tp = MATRIX.TOEPLITZ(c1, r1)
  PRINT "toep01   ="; MATRIX.GETVALUE(tp, 0, 1)
  PRINT "toep10   ="; MATRIX.GETVALUE(tp, 1, 0)
  hk = MATRIX.HANKEL(c1, r1)
  PRINT "hankel21 ="; MATRIX.GETVALUE(hk, 2, 1)
  cc = MATRIX.CIRCULANT(c1)
  PRINT "circ01   ="; MATRIX.GETVALUE(cc, 0, 1)

  ' solve_triangular: L = [[2,0],[1,2]], b = [4,5] -> x = [2,1.5]
  L = MATRIX.CREATE(2, 2)
  L = MATRIX.SETVALUE(L, 0, 0, 2)
  L = MATRIX.SETVALUE(L, 1, 0, 1) : L = MATRIX.SETVALUE(L, 1, 1, 2)
  DIM b(1)
  b(0) = 4 : b(1) = 5
  DIM vb() = LINALG.SOLVETRIANGULAR(L, b, 1)
  PRINT "st00     ="; vb(0)

  ' eig generale non simmetrica: A = [[1,2],[2,1]] -> autovalori 3, -1
  ed = LINALG.EIG(h)
  ev = LINALG.FACTOR(ed, "DREAL")
  PRINT "eigreal0 ="; MATRIX.GETVALUE(ev, 0, 0)
  PRINT "eigreal1 ="; MATRIX.GETVALUE(ev, 1, 0)
  vr = LINALG.FACTOR(ed, "VRREAL")
  PRINT "eigvr00  ="; MATRIX.GETVALUE(vr, 0, 0)
  ' schur: A = Z T Z^T
  sd = LINALG.SCHUR(h)
  t = LINALG.FACTOR(sd, "T")
  z = LINALG.FACTOR(sd, "Z")
  PRINT "schur00  ="; MATRIX.GETVALUE(t, 0, 0)
END SUB
