' MiniTest — LINALG quarta ondata: solver iterativi e power iteration.
' CG (SPD) e GMRES rendono le iterazioni; POWER rende l'autovalore dominante.
' EXPECT: cg= 2  x0= 1.181818  x1= 1.272727|gmres= 1  x0= 1  x1= 2|power= 0  lam= 2  v= 1
IMPORT LINALG

SUB Main()
  ' CG su SPD [[4,1],[1,3]] b=[6,5]: soluzione [13/11, 14/11] in 2 iterazioni
  DIM ca(3) AS DOUBLE
  ca(0)=4 : ca(1)=1 : ca(2)=1 : ca(3)=3
  DIM cb(1) AS DOUBLE, cx(1) AS DOUBLE
  cb(0)=6 : cb(1)=5
  DIM it AS LONG
  it = LINALG.CG(ca, 2, cb, cx, 50, 1e-12)
  PRINT "cg="; it; " x0="; ROUND(cx(0),6); " x1="; ROUND(cx(1),6)

  ' GMRES su [[1,2],[3,4]] b=[5,11]: soluzione [1,2]
  DIM ga(3) AS DOUBLE
  ga(0)=1 : ga(1)=2 : ga(2)=3 : ga(3)=4
  DIM gb(1) AS DOUBLE, gx(1) AS DOUBLE
  gb(0)=5 : gb(1)=11
  it = LINALG.GMRES(ga, 2, gb, gx, 2, 50, 1e-12)
  PRINT "gmres="; it; " x0="; ROUND(gx(0),6); " x1="; ROUND(gx(1),6)

  ' POWER su diag(2,1): autovalore dominante 2, v normalizzato (valore assoluto)
  DIM pa(3) AS DOUBLE
  pa(0)=2 : pa(1)=0 : pa(2)=0 : pa(3)=1
  DIM lam(0) AS DOUBLE, pv(1) AS DOUBLE
  PRINT "power="; LINALG.POWER(pa, 2, 200, 1e-10, lam, pv); " lam="; ROUND(lam(0),6); " v="; ROUND(ABS(pv(0)),6)
END SUB
