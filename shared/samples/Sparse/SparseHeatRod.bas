' FILE: Sparse/SparseHeatRod.bas
' SCENARIO DIDATTICO — temperatura di regime in una barra con una sorgente di
' calore uniforme. L'equazione -u''(x) = 1 con u(0) = u(1) = 0 si discretizza
' su 5 nodi interni (h = 1/6) e dà un sistema tridiagonale SPARSO:
'
'   K = (1/h^2) * tridiag(-1, 2, -1)      b = (1, 1, 1, 1, 1)
'
' La soluzione esatta è u(x) = x(1-x)/2, cioè ai nodi x = i/6:
'   u = (5/72, 8/72, 9/72, 8/72, 5/72) = (0.069444, 0.111111, 0.125, 0.111111, 0.069444)
'
' Il sistema si risolve due volte: con la LU sparsa (matrice completa) e con la
' Cholesky sparsa (triangolo inferiore). La matrice ha 13 non-zeri su 25 celle:
' è il caso in cui la rappresentazione sparsa serve davvero.
'
' Domini: SPARSE. Output deterministico.
IMPORT SPARSE

SUB Main()
  DIM n AS LONG
  DIM k AS LONG, i AS LONG, j AS LONG
  DIM h AS DOUBLE, kd AS DOUBLE, ko AS DOUBLE
  DIM b(4) AS DOUBLE
  DIM bs(4) AS DOUBLE
  DIM t(2) AS DOUBLE
  DIM sz(2) AS LONG
  DIM hcoo AS LONG, hcsc AS LONG, hf AS LONG
  DIM err AS DOUBLE, d AS DOUBLE

  n = 5
  h = 1 / 6
  kd = 2 / (h * h)          ' diagonale: 72
  ko = -1 / (h * h)         ' fuori diagonale: -36
  PRINT "barra: "; n; " nodi interni, h = "; FORMAT(h, "#.000000"); ", diagonale = "; FORMAT(kd, "#.0"); ", fuori = "; FORMAT(ko, "#.0")

  ' ── 1. matrice in COO: diagonale + due sottodiagonali ─────────────────────
  hcoo = SPARSE.COONEW(n, n, 3 * n - 2)
  k = 0
  FOR i = 0 TO n - 1
    SPARSE.COOSET(hcoo, k, i, i, kd)
    k = k + 1
  NEXT i
  FOR i = 1 TO n - 1
    SPARSE.COOSET(hcoo, k, i, i - 1, ko)
    k = k + 1
  NEXT i
  FOR i = 0 TO n - 2
    SPARSE.COOSET(hcoo, k, i, i + 1, ko)
    k = k + 1
  NEXT i
  PRINT "non-zeri in COO = "; k; " (la matrice densa ne avrebbe "; n * n; ")"

  ' ── 2. conversioni e ispezione ────────────────────────────────────────────
  hcsc = SPARSE.COOTOCSC(hcoo)
  SPARSE.SIZES(hcsc, sz)
  PRINT "CSC: righe="; sz(0); " colonne="; sz(1); " nnz="; sz(2)
  ' in CSC la prima colonna contiene (0,0) e (1,0)
  PRINT " prima entry = "; SPARSE.TRIPLET(hcsc, 0, t)
  PRINT " ("; FORMAT(t(0), "#.0"); ", "; FORMAT(t(1), "#.0"); ", "; FORMAT(t(2), "#.0"); ")"
  PRINT " seconda     = "; SPARSE.TRIPLET(hcsc, 1, t)
  PRINT " ("; FORMAT(t(0), "#.0"); ", "; FORMAT(t(1), "#.0"); ", "; FORMAT(t(2), "#.0"); ")"
  ' trasposta: la matrice è simmetrica, quindi cambia solo il formato
  DIM htr AS LONG
  htr = SPARSE.CSCTRANSPOSE(hcsc)
  SPARSE.SIZES(htr, sz)
  PRINT "trasposta (CSR): righe="; sz(0); " colonne="; sz(1); " nnz="; sz(2)

  ' ── 3. soluzione con la LU sparsa ─────────────────────────────────────────
  FOR i = 0 TO n - 1
    b(i) = 1
  NEXT i
  hf = SPARSE.SPLUFACTOR(hcsc)
  PRINT "LU: risolvo K u = b ... "; SPARSE.SPLUSOLVE(hf, b)
  FOR i = 0 TO n - 1
    PRINT " u("; i; ") = "; FORMAT(b(i), "#.000000")
  NEXT i
  PRINT "fattore liberato = "; SPARSE.FREE(hf)

  ' ── 4. la stessa soluzione con la Cholesky sparsa (triangolo inferiore) ───
  ' K è simmetrica definita positiva: bastano diagonale + sottodiagonale
  hcoo = SPARSE.COONEW(n, n, 2 * n - 1)
  k = 0
  FOR i = 0 TO n - 1
    SPARSE.COOSET(hcoo, k, i, i, kd)
    k = k + 1
  NEXT i
  FOR i = 1 TO n - 1
    SPARSE.COOSET(hcoo, k, i, i - 1, ko)
    k = k + 1
  NEXT i
  hcsc = SPARSE.COOTOCSC(hcoo)
  FOR i = 0 TO n - 1
    bs(i) = 1
  NEXT i
  hf = SPARSE.SPCHOLFACTOR(hcsc)
  SPARSE.SIZES(hcsc, sz)
  PRINT "Cholesky (triangolo inferiore, nnz="; sz(2); "): "; SPARSE.SPCHOLSOLVE(hf, bs)
  PRINT " liberato = "; SPARSE.FREE(hf)

  ' ── 5. confronto con la soluzione esatta u(x) = x(1-x)/2 ──────────────────
  err = 0
  FOR i = 0 TO n - 1
    d = MATH.ABS(bs(i) - b(i))
    IF d > err THEN err = d
  NEXT i
  PRINT "differenza LU - Cholesky = "; FORMAT(err, "#.000000000000")
  ' e rispetto alla formula: u(i) = x_i (1 - x_i) / 2 con x_i = (i+1)*h
  err = 0
  FOR i = 0 TO n - 1
    d = MATH.ABS(b(i) - ((i + 1) * h) * (1 - (i + 1) * h) / 2)
    IF d > err THEN err = d
  NEXT i
  PRINT "errore rispetto alla soluzione esatta = "; FORMAT(err, "#.000000000000")

  ' ── 6. guardie: matrice non definita positiva e handle inesistente ───────
  hcoo = SPARSE.COONEW(2, 2, 3)
  SPARSE.COOSET(hcoo, 0, 0, 0, 1) : SPARSE.COOSET(hcoo, 1, 1, 0, 2) : SPARSE.COOSET(hcoo, 2, 1, 1, 1)
  hcsc = SPARSE.COOTOCSC(hcoo)
  PRINT "non definita positiva -> Cholesky = "; SPARSE.SPCHOLFACTOR(hcsc)
  PRINT "handle inesistente -> "; SPARSE.SIZES(999, sz); ", "; SPARSE.FREE(999)
  PRINT "fine"
END SUB
