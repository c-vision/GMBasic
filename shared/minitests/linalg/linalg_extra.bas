' MiniTest — LINALG quarta ondata: traccia, spazi, minimi quadrati, tridiagonali,
' LDLᵀ, Gauss-Jordan, omega SOR. Tutto su buffer piatti row-major.
' EXPECT: trace= 5|rref= 1  o00= 1  o01= 2  o10= 0|colsp= 1  c0= 1  c1= 2|rowsp= 1  w0= 1  w1= 2|lstsq= 0  x0= 1  x1= 2|symtri= 0  d0= 4  e0=-1|trieig= 0  e0= 5.414214  e2= 2.585786|thomas= 0  x0= 1  x2= 1|ldlt= 0  l10= 0.5  d0= 4  d1= 2|ldlsolve= 0  x0= 1  x1= 1|gj= 0  x0= 1|omega= 1.071797
IMPORT LINALG

SUB Main()
  DIM a(3) AS DOUBLE
  a(0)=1 : a(1)=2 : a(2)=3 : a(3)=4
  PRINT "trace="; ROUND(LINALG.TRACE(a, 2), 6)

  ' RREF di [[1,2],[2,4]]: rango 1, seconda riga nulla
  DIM r(3) AS DOUBLE
  r(0)=1 : r(1)=2 : r(2)=2 : r(3)=4
  DIM ro(3) AS DOUBLE
  PRINT "rref="; LINALG.RREF(r, 2, 2, ro); " o00="; ROUND(ro(0),6); " o01="; ROUND(ro(1),6); " o10="; ROUND(ro(2),6)

  ' basi (segno libero: valori assoluti)
  DIM co(3) AS DOUBLE
  PRINT "colsp="; LINALG.COLSPACE(r, 2, 2, co); " c0="; ROUND(ABS(co(0)),6); " c1="; ROUND(ABS(co(1)),6)
  DIM rw(3) AS DOUBLE
  PRINT "rowsp="; LINALG.ROWSPACE(r, 2, 2, rw); " w0="; ROUND(ABS(rw(0)),6); " w1="; ROUND(ABS(rw(1)),6)

  ' minimi quadrati A=[[1,0],[0,1],[1,1]], b=[1,2,3] -> x=[1,2]
  DIM ls(5) AS DOUBLE
  ls(0)=1 : ls(1)=0
  ls(2)=0 : ls(3)=1
  ls(4)=1 : ls(5)=1
  DIM lb(2) AS DOUBLE
  lb(0)=1 : lb(1)=2 : lb(2)=3
  DIM lx(1) AS DOUBLE
  PRINT "lstsq="; LINALG.LSTSQ(ls, 3, 2, lb, lx); " x0="; ROUND(lx(0),6); " x1="; ROUND(lx(1),6)

  ' tridiagonale simmetrica [[4,1,0],[1,4,1],[0,1,4]]: d=[4,4,4], e[0]=-1
  DIM t(8) AS DOUBLE
  t(0)=4 : t(1)=1 : t(2)=0
  t(3)=1 : t(4)=4 : t(5)=1
  t(6)=0 : t(7)=1 : t(8)=4
  DIM dd(2) AS DOUBLE, ee(2) AS DOUBLE
  PRINT "symtri="; LINALG.SYMTRIDIAG(t, 3, dd, ee); " d0="; ROUND(dd(0),6); " e0="; ROUND(ee(0),6)
  DIM ev(2) AS DOUBLE
  PRINT "trieig="; LINALG.TRIDIAGEIG(dd, ee, 3, ev); " e0="; ROUND(ev(0),6); " e2="; ROUND(ev(2),6)

  ' Thomas [[2,1,0],[1,2,1],[0,1,2]] x = [3,4,3]
  DIM tl(2) AS DOUBLE, td(2) AS DOUBLE, tu(2) AS DOUBLE, tr(2) AS DOUBLE
  tl(0)=1 : tl(1)=1 : tl(2)=0
  td(0)=2 : td(1)=2 : td(2)=2
  tu(0)=1 : tu(1)=1 : tu(2)=0
  tr(0)=3 : tr(1)=4 : tr(2)=3
  DIM tx(2) AS DOUBLE
  PRINT "thomas="; LINALG.THOMAS(tl, td, tu, tr, 3, tx); " x0="; ROUND(tx(0),6); " x2="; ROUND(tx(2),6)

  ' LDLT di [[4,2],[2,3]] -> L=[[1,0],[0.5,1]], d=[4,2]; solve di [6,5] -> [1,1]
  DIM s(3) AS DOUBLE
  s(0)=4 : s(1)=2 : s(2)=2 : s(3)=3
  DIM ll(3) AS DOUBLE, ld(1) AS DOUBLE
  PRINT "ldlt="; LINALG.LDLT(s, 2, ll, ld); " l10="; ROUND(ll(2),6); " d0="; ROUND(ld(0),6); " d1="; ROUND(ld(1),6)
  DIM sb(1) AS DOUBLE, sx(1) AS DOUBLE
  sb(0)=6 : sb(1)=5
  PRINT "ldlsolve="; LINALG.LDLTSOLVE(ll, ld, sb, 2, sx); " x0="; ROUND(sx(0),6); " x1="; ROUND(sx(1),6)

  ' Gauss-Jordan sullo stesso sistema
  DIM gx(1) AS DOUBLE
  PRINT "gj="; LINALG.GAUSSJORDAN(s, 2, sb, gx); " x0="; ROUND(gx(0),6)

  ' omega ottimale SOR per [[2,-1],[-1,2]]
  DIM om(3) AS DOUBLE
  om(0)=2 : om(1)=-1 : om(2)=-1 : om(3)=2
  PRINT "omega="; ROUND(LINALG.OPTIMALOMEGA(om, 2), 6)
END SUB
