' EXPECT: ei= 0.308702|pi= 0.648027|ucb= 1.2|bidx= 1|bval= 7.282742|gp= 0.7|gd= 21|ipw= 1.989|atm= 1.966667
IMPORT MULTIVARIATE
SUB Main()
  PRINT "ei="; MATH.ROUND(MULTIVARIATE.BOEI(1.0, 0.5, 0.8, 0.01), 6)
  PRINT "pi="; MATH.ROUND(MULTIVARIATE.BOPI(1.0, 0.5, 0.8, 0.01), 6)
  PRINT "ucb="; MATH.ROUND(MULTIVARIATE.BOUCB(0.2, 0.5, 2.0), 6)
  DIM bx(4) AS DOUBLE
  DIM by(4) AS DOUBLE
  DIM bc(2) AS DOUBLE
  bx(0) = 0: bx(1) = 1: bx(2) = 2: bx(3) = 3
  by(0) = 0: by(1) = 1: by(2) = 4: by(3) = 9
  bc(0) = 0.5: bc(1) = 2.5
  DIM n = MULTIVARIATE.BONEXT(bx, by, bc, 1.0, 0.01, 2.0)
  PRINT "bidx="; n.Index
  PRINT "bval="; MATH.ROUND(n.Value, 6)
  PRINT "gp="; MATH.ROUND(MULTIVARIATE.GAMBLERRUINP(3, 10, 0.5), 6)
  PRINT "gd="; MATH.ROUND(MULTIVARIATE.GAMBLERRUIND(3, 10, 0.5), 6)
  DIM t(5) AS DOUBLE
  DIM y(5) AS DOUBLE
  DIM ps(5) AS DOUBLE
  FOR i = 0 TO 5
    t(i) = i MOD 2
    y(i) = 1.0 + 2.0 * t(i) + 0.1 * i
    ps(i) = 0.3 + 0.08 * i
  NEXT i
  PRINT "ipw="; MATH.ROUND(MULTIVARIATE.ATEIPW(t, y, ps), 6)
  PRINT "atm="; MATH.ROUND(MULTIVARIATE.ATEMATCHING(t, y, ps), 6)
END SUB
