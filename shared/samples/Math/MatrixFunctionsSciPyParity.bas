' scipy.linalg completato: sqrtm, logm, hessenberg, block_diag + SETVALUE/GETVALUE.
' Oracoli: scipy 1.18 (sqrtm([[2,1],[1,2]]) diag 1.3660, logm diag 0.5493, hessenberg[2,0]=0).

SUB Main()
  ' A = [[2,1],[1,2]]
  h = MATRIX.CREATE(2, 2)
  h = MATRIX.SETVALUE(h, 0, 0, 2) : h = MATRIX.SETVALUE(h, 0, 1, 1)
  h = MATRIX.SETVALUE(h, 1, 0, 1) : h = MATRIX.SETVALUE(h, 1, 1, 2)

  s = LINALG_MATRIX.SQRTM(h)
  PRINT "sqrtm00  ="; MATRIX.GETVALUE(s, 0, 0)
  PRINT "sqrtm01  ="; MATRIX.GETVALUE(s, 0, 1)

  l = LINALG_MATRIX.LOGM(h)
  PRINT "logm00   ="; MATRIX.GETVALUE(l, 0, 0)
  PRINT "logm11   ="; MATRIX.GETVALUE(l, 1, 1)

  ' Hessenberg di una 3x3: Hh[2,0] deve essere ~0
  h3 = MATRIX.CREATE(3, 3)
  h3 = MATRIX.SETVALUE(h3, 0, 0, 1) : h3 = MATRIX.SETVALUE(h3, 0, 1, 2) : h3 = MATRIX.SETVALUE(h3, 0, 2, 3)
  h3 = MATRIX.SETVALUE(h3, 1, 0, 4) : h3 = MATRIX.SETVALUE(h3, 1, 1, 5) : h3 = MATRIX.SETVALUE(h3, 1, 2, 6)
  h3 = MATRIX.SETVALUE(h3, 2, 0, 7) : h3 = MATRIX.SETVALUE(h3, 2, 1, 8) : h3 = MATRIX.SETVALUE(h3, 2, 2, 10)
  hh = LINALG_MATRIX.HESSENBERG(h3)
  PRINT "hess20   ="; MATRIX.GETVALUE(hh, 2, 0)

  ' block_diag
  bd = MATRIX.BLOCKDIAG(h, h)
  PRINT "bd00     ="; MATRIX.GETVALUE(bd, 0, 0)
  PRINT "bd22     ="; MATRIX.GETVALUE(bd, 2, 2)

  ' Pseudoinversa Moore-Penrose (per A invertibile coincide con l'inversa)
  p = LINALG_MATRIX.PINVERSE(h)
  PRINT "pinv00   ="; MATRIX.GETVALUE(p, 0, 0)
  PRINT "pinv11   ="; MATRIX.GETVALUE(p, 1, 1)
END SUB
