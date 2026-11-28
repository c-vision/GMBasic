' scipy.linalg: sqrtm, logm, hessenberg, pinv (LINALG FFI) + block_diag MATRIX.
' Oracoli: scipy 1.18 (sqrtm([[2,1],[1,2]]) diag 1.3660, logm diag 0.5493, hessenberg[2,0]=0).
IMPORT LINALG
SUB Main()
  DIM a(3) AS DOUBLE
  a(0)=2 : a(1)=1 : a(2)=1 : a(3)=2

  DIM s(3) AS DOUBLE
  LINALG.SQRTM(a, 2, s)
  PRINT "sqrtm00  ="; s(0)
  PRINT "sqrtm01  ="; s(1)

  DIM l(3) AS DOUBLE
  LINALG.LOGM(a, 2, l)
  PRINT "logm00   ="; l(0)
  PRINT "logm11   ="; l(3)

  ' Hessenberg di una 3x3: Hh[2,0] deve essere ~0
  DIM h3(8) AS DOUBLE
  h3(0)=1 : h3(1)=2 : h3(2)=3
  h3(3)=4 : h3(4)=5 : h3(5)=6
  h3(6)=7 : h3(7)=8 : h3(8)=10
  DIM hh(8) AS DOUBLE
  LINALG.HESSENBERG(h3, 3, hh)
  PRINT "hess20   ="; hh(6)

  ' block_diag (MATRIX)
  DIM h = MATRIX.CREATE(2, 2)
  h = MATRIX.SETVALUE(h, 0, 0, 2) : h = MATRIX.SETVALUE(h, 0, 1, 1)
  h = MATRIX.SETVALUE(h, 1, 0, 1) : h = MATRIX.SETVALUE(h, 1, 1, 2)
  DIM bd = MATRIX.BLOCKDIAG(h, h)
  PRINT "bd00     ="; MATRIX.GETVALUE(bd, 0, 0)
  PRINT "bd22     ="; MATRIX.GETVALUE(bd, 2, 2)

  DIM p(3) AS DOUBLE
  LINALG.PINVERSE(a, 2, 2, p)
  PRINT "pinv00   ="; p(0)
  PRINT "pinv11   ="; p(3)
END SUB
