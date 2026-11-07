' SPARSE (FFI) — contenitori COO/CSR/CSC, conversioni e solutori diretti.
' Matrice di prova K = [[4,1,0],[1,3,1],[0,1,2]] (simmetrica definita positiva).
' Sistema K x = (1,2,3): soluzione a mano x = (2/9, 1/9, 13/9) = (0.2222, 0.1111, 1.4444).
' EXPECT: sizes= 0|: 3 , 3 , 7||t0= 0|:0.0,0.0,4.0||t1= 0|:0.0,1.0,1.0||csc-sizes= 0|: 3 , 3 , 7||c0= 0|:0.0,0.0||c1= 0|:1.0,0.0||back-sizes= 0|: 3 , 3 , 7||back-t0= 0|:0.0,0.0,4.0||tr-sizes= 0|: 3 , 2 , 4||tr2= 0|:1.0,1.0,3.0|coo-set= 0||lower-sizes= 0|: 3 , 3 , 5|chol-solve= 0|:0.222222,0.111111,1.444444||free= 0||lu-solve= 0|:0.222222,0.111111,1.444444||free= 0||notspd=-1||sing=-1||bad=-1 :-1||f=0
IMPORT SPARSE

SUB Main()
  DIM d(8) AS DOUBLE
  DIM b(2) AS DOUBLE
  DIM t(2) AS DOUBLE
  DIM sz(2) AS LONG
  DIM hcoo AS LONG, hcsr AS LONG, hcsc AS LONG, h2 AS LONG, hf AS LONG

  ' ── densa -> CSR: gli zeri si scartano (7 non-zeri su 9) ──────────────────
  d(0) = 4 : d(1) = 1 : d(2) = 0
  d(3) = 1 : d(4) = 3 : d(5) = 1
  d(6) = 0 : d(7) = 1 : d(8) = 2
  hcsr = SPARSE.DENSETOCSR(3, 3, d)
  PRINT "sizes="; SPARSE.SIZES(hcsr, sz)
  PRINT ":"; sz(0); ","; sz(1); ","; sz(2)
  ' riga 0: (0,0,4) e (0,1,1); riga 1: (1,0,1), (1,1,3), (1,2,1)
  PRINT "|t0="; SPARSE.TRIPLET(hcsr, 0, t)
  PRINT ":"; FORMAT(t(0), "#.0"); ","; FORMAT(t(1), "#.0"); ","; FORMAT(t(2), "#.0")
  PRINT "|t1="; SPARSE.TRIPLET(hcsr, 1, t)
  PRINT ":"; FORMAT(t(0), "#.0"); ","; FORMAT(t(1), "#.0"); ","; FORMAT(t(2), "#.0")

  ' ── conversioni: CSR -> CSC -> CSR (stessa matrice) ───────────────────────
  hcsc = SPARSE.CSRTOCSC(hcsr)
  PRINT "|csc-sizes="; SPARSE.SIZES(hcsc, sz)
  PRINT ":"; sz(0); ","; sz(1); ","; sz(2)
  ' in CSC le entry di una colonna sono contigue: colonna 0 = (0,0,4),(1,0,1)
  PRINT "|c0="; SPARSE.TRIPLET(hcsc, 0, t)
  PRINT ":"; FORMAT(t(0), "#.0"); ","; FORMAT(t(1), "#.0")
  PRINT "|c1="; SPARSE.TRIPLET(hcsc, 1, t)
  PRINT ":"; FORMAT(t(0), "#.0"); ","; FORMAT(t(1), "#.0")
  h2 = SPARSE.CSCTOCSR(hcsc)
  PRINT "|back-sizes="; SPARSE.SIZES(h2, sz)
  PRINT ":"; sz(0); ","; sz(1); ","; sz(2)
  PRINT "|back-t0="; SPARSE.TRIPLET(h2, 0, t)
  PRINT ":"; FORMAT(t(0), "#.0"); ","; FORMAT(t(1), "#.0"); ","; FORMAT(t(2), "#.0")

  ' ── trasposta di una matrice NON simmetrica ───────────────────────────────
  ' A = [[1,2,0],[0,3,4]] (2x3, 4 non-zeri) -> A^T (3x2): (0,0,1),(1,0,2),(1,1,3),(2,1,4)
  DIM a(5) AS DOUBLE
  a(0) = 1 : a(1) = 2 : a(2) = 0
  a(3) = 0 : a(4) = 3 : a(5) = 4
  hcsr = SPARSE.DENSETOCSR(2, 3, a)
  h2 = SPARSE.CSRTRANSPOSE(hcsr)
  PRINT "|tr-sizes="; SPARSE.SIZES(h2, sz)
  PRINT ":"; sz(0); ","; sz(1); ","; sz(2)
  PRINT "|tr2="; SPARSE.TRIPLET(h2, 2, t)
  PRINT ":"; FORMAT(t(0), "#.0"); ","; FORMAT(t(1), "#.0"); ","; FORMAT(t(2), "#.0")

  ' ── COO a mano -> CSC del triangolo inferiore di K ────────────────────────
  hcoo = SPARSE.COONEW(3, 3, 5)
  SPARSE.COOSET(hcoo, 0, 0, 0, 4)
  SPARSE.COOSET(hcoo, 1, 1, 0, 1)
  SPARSE.COOSET(hcoo, 2, 1, 1, 3)
  SPARSE.COOSET(hcoo, 3, 2, 1, 1)
  PRINT "coo-set="; SPARSE.COOSET(hcoo, 4, 2, 2, 2)
  hcsc = SPARSE.COOTOCSC(hcoo)
  PRINT "|lower-sizes="; SPARSE.SIZES(hcsc, sz)
  PRINT ":"; sz(0); ","; sz(1); ","; sz(2)

  ' ── Cholesky sparsa: K x = (1,2,3) ────────────────────────────────────────
  b(0) = 1 : b(1) = 2 : b(2) = 3
  hf = SPARSE.SPCHOLFACTOR(hcsc)
  PRINT "chol-solve="; SPARSE.SPCHOLSOLVE(hf, b)
  PRINT ":"; FORMAT(b(0), "#.000000"); ","; FORMAT(b(1), "#.000000"); ","; FORMAT(b(2), "#.000000")
  PRINT "|free="; SPARSE.FREE(hf)

  ' ── LU sparsa sulla matrice completa (CSC da CSR) ────────────────────────
  hcsr = SPARSE.DENSETOCSR(3, 3, d)
  hcsc = SPARSE.CSRTOCSC(hcsr)
  b(0) = 1 : b(1) = 2 : b(2) = 3
  hf = SPARSE.SPLUFACTOR(hcsc)
  PRINT "|lu-solve="; SPARSE.SPLUSOLVE(hf, b)
  PRINT ":"; FORMAT(b(0), "#.000000"); ","; FORMAT(b(1), "#.000000"); ","; FORMAT(b(2), "#.000000")
  PRINT "|free="; SPARSE.FREE(hf)

  ' ── guardie ───────────────────────────────────────────────────────────────
  ' matrice non definita positiva [[1,2],[2,1]] (autovalori 3 e -1)
  DIM npd(3) AS DOUBLE
  npd(0) = 1 : npd(1) = 0 : npd(2) = 2 : npd(3) = 1
  hcoo = SPARSE.COONEW(2, 2, 3)
  SPARSE.COOSET(hcoo, 0, 0, 0, 1) : SPARSE.COOSET(hcoo, 1, 1, 0, 2) : SPARSE.COOSET(hcoo, 2, 1, 1, 1)
  hcsc = SPARSE.COOTOCSC(hcoo)
  PRINT "|notspd="; SPARSE.SPCHOLFACTOR(hcsc)
  ' matrice singolare [[1,2],[2,4]]
  hcoo = SPARSE.COONEW(2, 2, 4)
  SPARSE.COOSET(hcoo, 0, 0, 0, 1) : SPARSE.COOSET(hcoo, 1, 0, 1, 2)
  SPARSE.COOSET(hcoo, 2, 1, 0, 2) : SPARSE.COOSET(hcoo, 3, 1, 1, 4)
  hcsc = SPARSE.COOTOCSC(hcoo)
  PRINT "|sing="; SPARSE.SPLUFACTOR(hcsc)
  ' handle inesistente
  PRINT "|bad="; SPARSE.SIZES(999, sz); ":"; SPARSE.FREE(999)
  PRINT "|f=0"
END SUB
