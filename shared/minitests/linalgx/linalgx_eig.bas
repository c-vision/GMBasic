' MiniTest — LINALGX terza ondata: autovalori/autovettori e angoli fra sottospazi.
' EVD simmetrica (d ascendenti, V colonne ortonormali), EIG generale (autovalori
' reali/immaginari e autovettori destri: residuo ||A·v - λv|| ~ 0), SUBSPACEANGLES.
' EXPECT: evd= 0  d0= 1  d1= 3  orth= 0|eig= 0  wr0= 0  wr1= 0  wi= 1|res= 0|sub= 1  ang0= 0.785398
IMPORT LINALGX

SUB Main()
  ' A = [[2,1],[1,2]] simmetrica: autovalori 1 e 3, autovettori ortonormali
  DIM a(3) AS DOUBLE
  a(0)=2 : a(1)=1 : a(2)=1 : a(3)=2
  DIM d(1) AS DOUBLE, V(3) AS DOUBLE
  DIM rc AS LONG
  rc = LINALGX.EVD(a, 2, d, V)
  PRINT "evd="; rc; " d0="; ROUND(d(0),6); " d1="; ROUND(d(1),6); " orth="; ROUND(V(0)*V(1)+V(2)*V(3),6)

  ' rotazione [[0,-1],[1,0]]: autovalori ±i (parte reale 0, |parte immaginaria| 1)
  DIM r(3) AS DOUBLE
  r(0)=0 : r(1)=-1 : r(2)=1 : r(3)=0
  DIM wr(1) AS DOUBLE, wi(1) AS DOUBLE, vr(3) AS DOUBLE, vi(3) AS DOUBLE
  rc = LINALGX.EIG(r, 2, wr, wi, vr, vi)
  PRINT "eig="; rc; " wr0="; ROUND(wr(0),6); " wr1="; ROUND(wr(1),6); " wi="; ROUND(ABS(wi(0)),6)

  ' matrice non simmetrica con autovalori reali [[4,1],[2,3]]: 2 e 5.
  ' Residuo del primo autovettore: ||A·v - wr0·v|| deve essere 0.
  DIM n(3) AS DOUBLE
  n(0)=4 : n(1)=1 : n(2)=2 : n(3)=3
  DIM w2(1) AS DOUBLE, i2(1) AS DOUBLE, v2(3) AS DOUBLE, m2(3) AS DOUBLE
  rc = LINALGX.EIG(n, 2, w2, i2, v2, m2)
  DIM e0 AS DOUBLE, e1 AS DOUBLE
  e0 = (n(0)*v2(0) + n(1)*v2(2)) - w2(0)*v2(0)
  e1 = (n(2)*v2(0) + n(3)*v2(2)) - w2(0)*v2(2)
  PRINT "res="; ROUND(ABS(e0)+ABS(e1),6)

  ' sottospazi: col(A) = (1,0), col(B) = (1,1) -> 45 gradi
  DIM u(1) AS DOUBLE
  u(0)=1 : u(1)=0
  DIM w(1) AS DOUBLE
  w(0)=1 : w(1)=1
  DIM ang(0) AS DOUBLE
  DIM na AS LONG
  na = LINALGX.SUBSPACEANGLES(u, 2, 1, w, 2, 1, ang)
  PRINT "sub="; na; " ang0="; ROUND(ang(0),6)
END SUB
