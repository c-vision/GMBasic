' EXPECT: det= 3|trace= 4|rank= 2|norm= 3|cond= 3|inv00= 0.666667|mul00= 5|sq00= 1.366025|lg00= 0.549306|ex00= 11.401909
IMPORT LINALG
SUB Main()
  DIM a(3) AS DOUBLE
  a(0)=2 : a(1)=1 : a(2)=1 : a(3)=2
  DIM h = MATRIX.CREATE(2,2)
  h = MATRIX.SETVALUE(h, 0, 0, 2) : h = MATRIX.SETVALUE(h, 0, 1, 1)
  h = MATRIX.SETVALUE(h, 1, 0, 1) : h = MATRIX.SETVALUE(h, 1, 1, 2)
  PRINT "det="; MATH.ROUND(LINALG.DETERMINANT(a, 2), 6)
  PRINT "trace="; MATH.ROUND(MATRIX.TRACE(h), 6)
  PRINT "rank="; LINALG.RANK(a, 2, 2)
  PRINT "norm="; MATH.ROUND(LINALG.NORM(a, 2, 2), 6)
  PRINT "cond="; MATH.ROUND(LINALG.COND(a, 2, 2), 4)
  DIM inv(3) AS DOUBLE
  LINALG.INVERSE(a, 2, inv)
  PRINT "inv00="; MATH.ROUND(inv(0), 6)
  DIM mul = MATRIX.MULTIPLY(h, h)
  PRINT "mul00="; MATH.ROUND(MATRIX.GETVALUE(mul, 0, 0), 6)
  DIM sq(3) AS DOUBLE
  LINALG.SQRTM(a, 2, sq)
  PRINT "sq00="; MATH.ROUND(sq(0), 6)
  DIM lg(3) AS DOUBLE
  LINALG.LOGM(a, 2, lg)
  PRINT "lg00="; MATH.ROUND(lg(0), 6)
  DIM ex(3) AS DOUBLE
  LINALG.EXPM(a, 2, ex)
  PRINT "ex00="; MATH.ROUND(ex(0), 6)
END SUB
