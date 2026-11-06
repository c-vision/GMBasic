' EXPECT: cov= 3.333333|neg= -3.333333
IMPORT STATSX
SUB Main()
  DIM x() = {1, 2, 3, 4}
  DIM y() = {2, 4, 6, 8}
  DIM z() = {8, 6, 4, 2}
  PRINT "cov="; MATH.ROUND(STATSX.COVARIANCE(x, y, 1), 6)
  PRINT "neg="; MATH.ROUND(STATSX.COVARIANCE(x, z, 1), 6)
END SUB