' EXPECT: lu= 560|pol= 560|inv= 52.5|pin= 52.5
' Ciclo lungo di decomposizioni/operazioni matriciali: i fattori di una decomposizione
' vivono nel registry matrici (256 slot) e le decomposizioni in quello dedicato (128).
' Il rilascio dei fattori + l'eviction FIFO devono impedire l'esaurimento (prima il
' valore diventava 0 dopo ~30-110 iterazioni su C99/VM-C).
IMPORT SYSTEM

SUB Main()
  DIM a(1, 1) AS DOUBLE
  a(0, 0) = 4 : a(0, 1) = 2
  a(1, 0) = 2 : a(1, 1) = 3
  DIM i AS LONG
  DIM s1 AS DOUBLE, s2 AS DOUBLE, s3 AS DOUBLE, s4 AS DOUBLE
  FOR i = 1 TO 140
    DIM lu = LINALG_MATRIX.LU(a)
    s1 = s1 + lu.U(0, 0)
    DIM po = LINALG_MATRIX.POLAR(a)
    s2 = s2 + po.P(0, 0)
    DIM inv = LINALG_MATRIX.INVERSE(a)
    s3 = s3 + inv(0, 0)
    DIM pin = LINALG_MATRIX.PINVERSE(a)
    s4 = s4 + pin(0, 0)
  NEXT i
  PRINT "lu="; s1
  PRINT "pol="; s2
  PRINT "inv="; s3
  PRINT "pin="; s4
END SUB
