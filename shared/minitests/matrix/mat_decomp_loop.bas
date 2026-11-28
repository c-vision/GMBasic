' EXPECT: lu= 560|pol= 560|inv= 52.5|pin= 52.5
' Ciclo lungo di decomposizioni FFI: nessun registry di handle/decomposizioni,
' solo buffer locali (verifica che 140 iterazioni non derubino risorse).
IMPORT LINALG
SUB Main()
  DIM a(3) AS DOUBLE
  a(0)=4 : a(1)=2 : a(2)=2 : a(3)=3
  DIM i AS LONG
  DIM s1 AS DOUBLE, s2 AS DOUBLE, s3 AS DOUBLE, s4 AS DOUBLE
  DIM l(3) AS DOUBLE, u(3) AS DOUBLE, p(3) AS DOUBLE
  DIM pu(3) AS DOUBLE, pp(3) AS DOUBLE
  DIM inv(3) AS DOUBLE
  DIM pin(3) AS DOUBLE
  FOR i = 1 TO 140
    LINALG.LU(a, 2, l, u, p)
    s1 = s1 + u(0)
    LINALG.POLAR(a, 2, pu, pp)
    s2 = s2 + pp(0)
    LINALG.INVERSE(a, 2, inv)
    s3 = s3 + inv(0)
    LINALG.PINVERSE(a, 2, 2, pin)
    s4 = s4 + pin(0)
  NEXT i
  PRINT "lu="; s1
  PRINT "pol="; s2
  PRINT "inv="; s3
  PRINT "pin="; s4
END SUB
