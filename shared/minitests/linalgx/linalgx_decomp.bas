' MiniTest — LINALGX seconda ondata: decomposizioni su array piatti row-major.
' A = P·L·U (LU), A = Q·R (QR), A = U·diag(S)·Vᵀ (SVD), A = L·Lᵀ (Cholesky),
' A = Q·H·Qᵀ (Hessenberg), A = Z·T·Zᵀ (Schur), pseudoinversa.
' EXPECT: chol= 0  l00= 2  l10= 0.5  l11= 1.658312|lu= 0  L10= 0.333333  U00= 3  U11= 0.666667  P00= 0  P10= 1|qr= 0  q00= 0.316228  r00= 3.162278  r01= 4.427189|svd= 0  s0= 3  s1= 1|hess= 0  h20= 0  h10=-8.062258|schur= 0  t10= 1  t00= 2|pinv= 0  p00=-2  p10= 1.5
IMPORT LINALGX

SUB Main()
  ' A SPD [[4,1],[1,3]] -> L = [[2,0],[0.5,sqrt(2.75)]]
  DIM a(3) AS DOUBLE
  a(0)=4 : a(1)=1 : a(2)=1 : a(3)=3
  DIM L(3) AS DOUBLE
  DIM rc AS LONG
  rc = LINALGX.CHOLESKY(a, 2, L)
  PRINT "chol="; rc; " l00="; ROUND(L(0),6); " l10="; ROUND(L(2),6); " l11="; ROUND(L(3),6)

  ' B = [[1,2],[3,4]]: LU con pivot -> P=[[0,1],[1,0]], L=[[1,0],[1/3,1]], U=[[3,4],[0,2/3]]
  DIM b(3) AS DOUBLE
  b(0)=1 : b(1)=2 : b(2)=3 : b(3)=4
  DIM LL(3) AS DOUBLE, U(3) AS DOUBLE, P(3) AS DOUBLE
  rc = LINALGX.LU(b, 2, LL, U, P)
  PRINT "lu="; rc; " L10="; ROUND(LL(2),6); " U00="; ROUND(U(0),6); " U11="; ROUND(U(3),6); " P00="; ROUND(P(0),6); " P10="; ROUND(P(2),6)

  DIM Q(3) AS DOUBLE, R(3) AS DOUBLE
  rc = LINALGX.QR(b, 2, 2, Q, R)
  PRINT "qr="; rc; " q00="; ROUND(Q(0),6); " r00="; ROUND(R(0),6); " r01="; ROUND(R(1),6)

  ' C = [[2,1],[1,2]] SPD -> valori singolari 3 e 1
  DIM c(3) AS DOUBLE
  c(0)=2 : c(1)=1 : c(2)=1 : c(3)=2
  DIM UU(3) AS DOUBLE, S(1) AS DOUBLE, VT(3) AS DOUBLE
  rc = LINALGX.SVD(c, 2, 2, UU, S, VT)
  PRINT "svd="; rc; " s0="; ROUND(S(0),6); " s1="; ROUND(S(1),6)

  ' Hessenberg 3x3: H(2,0) deve essere 0 (forma di Hessenberg)
  DIM h(8) AS DOUBLE
  h(0)=1 : h(1)=2 : h(2)=3
  h(3)=4 : h(4)=5 : h(5)=6
  h(6)=7 : h(7)=8 : h(8)=9
  DIM HH(8) AS DOUBLE
  rc = LINALGX.HESSENBERG(h, 3, HH)
  PRINT "hess="; rc; " h20="; ROUND(HH(6),6); " h10="; ROUND(HH(3),6)

  ' Schur reale 2x2 (coppia complessa: blocco quasi-triangolare [2 1; 1 2]? no:
  ' A=[[2,1],[1,2]] ha autovalori reali; il kernel lascia il blocco 2x2 -> t10=1)
  DIM TT(3) AS DOUBLE, ZZ(3) AS DOUBLE
  rc = LINALGX.SCHUR(c, 2, TT, ZZ)
  PRINT "schur="; rc; " t10="; ROUND(TT(2),6); " t00="; ROUND(TT(0),6)

  ' pseudoinversa di [[1,2],[3,4]] = inv = [[-2,1],[1.5,-0.5]]
  DIM PI(3) AS DOUBLE
  rc = LINALGX.PINVERSE(b, 2, 2, PI)
  PRINT "pinv="; rc; " p00="; ROUND(PI(0),6); " p10="; ROUND(PI(2),6)
END SUB
