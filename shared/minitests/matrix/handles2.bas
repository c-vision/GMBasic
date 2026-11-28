' EXPECT: hh00= 2|co00=-0.224845|si00= 0.491295|sg00= 1
IMPORT LINALG
SUB Main()
  DIM a(3) AS DOUBLE
  a(0)=2 : a(1)=1 : a(2)=1 : a(3)=2
  DIM hh(3) AS DOUBLE
  LINALG.HESSENBERG(a, 2, hh)
  PRINT "hh00="; MATH.ROUND(hh(0), 6)
  DIM co(3) AS DOUBLE
  LINALG.COSM(a, 2, co)
  PRINT "co00="; MATH.ROUND(co(0), 6)
  DIM si(3) AS DOUBLE
  LINALG.SINM(a, 2, si)
  PRINT "si00="; MATH.ROUND(si(0), 6)
  DIM sg(3) AS DOUBLE
  LINALG.SIGNM(a, 2, sg)
  PRINT "sg00="; MATH.ROUND(sg(0), 6)
END SUB
