' MiniTest — LINALGX terza ondata: funzioni matriciali eigenwise (EVD simmetrica).
' SINM/COSM/TANM/SIGNM/FRACTIONALPOWER su A = [[2,1],[1,2]] (autovalori 1,3);
' SINM su [[0,1],[1,0]] (autovalori ±1) interscambia le righe come SIGNM.
' EXPECT: signm= 0  s00= 0  s01= 1|cosm= 0  c00=-0.224845  c01=-0.765147|sinm= 0  s00= 0.491295  s01=-0.350175|tanm= 0  t00= 0.707431|fp= 0  f00= 1.366025  f01= 0.366025
IMPORT LINALGX

SUB Main()
  ' A = [[2,1],[1,2]]
  DIM a(3) AS DOUBLE
  a(0)=2 : a(1)=1 : a(2)=1 : a(3)=2
  DIM rc AS LONG

  ' signm([[0,1],[1,0]]) = la matrice stessa (autovalori ±1)
  DIM s(3) AS DOUBLE
  s(0)=0 : s(1)=1 : s(2)=1 : s(3)=0
  DIM SG(3) AS DOUBLE
  rc = LINALGX.SIGNM(s, 2, SG)
  PRINT "signm="; rc; " s00="; ROUND(SG(0),6); " s01="; ROUND(SG(1),6)

  DIM C(3) AS DOUBLE
  rc = LINALGX.COSM(a, 2, C)
  PRINT "cosm="; rc; " c00="; ROUND(C(0),6); " c01="; ROUND(C(1),6)

  DIM SI(3) AS DOUBLE
  rc = LINALGX.SINM(a, 2, SI)
  PRINT "sinm="; rc; " s00="; ROUND(SI(0),6); " s01="; ROUND(SI(1),6)

  DIM T(3) AS DOUBLE
  rc = LINALGX.TANM(a, 2, T)
  PRINT "tanm="; rc; " t00="; ROUND(ABS(T(0)),6)

  ' radice quadrata principale: [[1.366025,0.366025],[0.366025,1.366025]]
  DIM F(3) AS DOUBLE
  rc = LINALGX.FRACTIONALPOWER(a, 2, 0.5, F)
  PRINT "fp="; rc; " f00="; ROUND(F(0),6); " f01="; ROUND(F(1),6)
END SUB
