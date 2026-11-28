' MiniTest — LINALGX seconda ondata: funzioni matriciali e basi (array piatti).
' EXPM/LOGM/SQRTM su matrici diagonali/semplici, POLAR, ORTH (proiettore Q·Qᵀ,
' base-independent), NULLSPACE (valori assoluti: il segno della base è libero).
' EXPECT: expm= 0  e00= 1  e01= 1  e10= 0  e11= 1|sqrtm= 0  s00= 2  s01= 0  s11= 3|logm= 0  l00= 0  l11= 1|polar= 0  u00= 1  u01= 0  u11= 1  p00= 2  p11= 3|orth= 2|proj= 0.666667 , 0.333333 ,-0.333333 , 0.666667 , 0.333333 , 0.666667|null= 1  n0= 0.894427  n1= 0.447214
IMPORT LINALGX

SUB Main()
  ' exp([[0,1],[0,0]]) = [[1,1],[0,1]]
  DIM e(3) AS DOUBLE
  e(0)=0 : e(1)=1 : e(2)=0 : e(3)=0
  DIM EX(3) AS DOUBLE
  DIM rc AS LONG
  rc = LINALGX.EXPM(e, 2, EX)
  PRINT "expm="; rc; " e00="; ROUND(EX(0),6); " e01="; ROUND(EX(1),6); " e10="; ROUND(EX(2),6); " e11="; ROUND(EX(3),6)

  ' sqrt(diag(4,9)) = diag(2,3)
  DIM s(3) AS DOUBLE
  s(0)=4 : s(1)=0 : s(2)=0 : s(3)=9
  DIM S2(3) AS DOUBLE
  rc = LINALGX.SQRTM(s, 2, S2)
  PRINT "sqrtm="; rc; " s00="; ROUND(S2(0),6); " s01="; ROUND(S2(1),6); " s11="; ROUND(S2(3),6)

  ' log(diag(1,e)) = diag(0,1)
  DIM lg(3) AS DOUBLE
  lg(0)=1 : lg(1)=0 : lg(2)=0 : lg(3)=2.718281828459045
  DIM LGO(3) AS DOUBLE
  rc = LINALGX.LOGM(lg, 2, LGO)
  PRINT "logm="; rc; " l00="; ROUND(LGO(0),6); " l11="; ROUND(LGO(3),6)

  ' polar(diag(2,3)): U=I, P=diag(2,3)
  DIM pd(3) AS DOUBLE
  pd(0)=2 : pd(1)=0 : pd(2)=0 : pd(3)=3
  DIM PU(3) AS DOUBLE, PP(3) AS DOUBLE
  rc = LINALGX.POLAR(pd, 2, PU, PP)
  PRINT "polar="; rc; " u00="; ROUND(PU(0),6); " u01="; ROUND(PU(1),6); " u11="; ROUND(PU(3),6); " p00="; ROUND(PP(0),6); " p11="; ROUND(PP(3),6)

  ' ORTH di [[1,0],[1,1],[0,1]] (3x2): base ortonormale dello spazio delle colonne.
  ' Il proiettore Q*Qᵀ è unico (indipendente dalla base scelta).
  DIM o(5) AS DOUBLE
  o(0)=1 : o(1)=0
  o(2)=1 : o(3)=1
  o(4)=0 : o(5)=1
  DIM OO(5) AS DOUBLE
  rc = LINALGX.ORTH(o, 3, 2, OO)
  PRINT "orth="; rc
  DIM r AS LONG
  DIM p00 AS DOUBLE, p01 AS DOUBLE, p02 AS DOUBLE
  DIM p11 AS DOUBLE, p12 AS DOUBLE, p22 AS DOUBLE
  p00=0 : p01=0 : p02=0 : p11=0 : p12=0 : p22=0
  FOR r = 0 TO 1
    p00 = p00 + OO(0*2 + r) * OO(0*2 + r)
    p01 = p01 + OO(0*2 + r) * OO(1*2 + r)
    p02 = p02 + OO(0*2 + r) * OO(2*2 + r)
    p11 = p11 + OO(1*2 + r) * OO(1*2 + r)
    p12 = p12 + OO(1*2 + r) * OO(2*2 + r)
    p22 = p22 + OO(2*2 + r) * OO(2*2 + r)
  NEXT r
  PRINT "proj="; ROUND(p00,6); ","; ROUND(p01,6); ","; ROUND(p02,6); ","; ROUND(p11,6); ","; ROUND(p12,6); ","; ROUND(p22,6)

  ' NULLSPACE di [[1,2],[2,4]] (rango 1): [2,-1]/sqrt(5); il segno è libero.
  DIM n2(3) AS DOUBLE
  n2(0)=1 : n2(1)=2 : n2(2)=2 : n2(3)=4
  DIM N(1) AS DOUBLE
  rc = LINALGX.NULLSPACE(n2, 2, 2, N)
  PRINT "null="; rc; " n0="; ROUND(ABS(N(0)),6); " n1="; ROUND(ABS(N(1)),6)
END SUB
