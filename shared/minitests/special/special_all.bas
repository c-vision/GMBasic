' MiniTest NAMESPACE SPECIAL — tutte le 19 funzioni speciali della superficie.
' Input identici ai MiniTest per-funzione (port MathNet / AMOS f2c, bit-esatti).
' EXPECT: besseli= 1.26606587775201|besselj= 0.765197686557967|besselk= 0.421024438240708|bessely= 0.088256964215677|beta= 0.0833333333333333|betainc= 0.6875|betaincinv= 0.385727568132389|betaln=-2.484906649788|gamma= 24.0000000000001|gammainc= 0.800851726528544|gammaincc= 0.199148273471456|gammaincinv= 1.67834699001666|gammainccinv= 1.67834699001666|lgamma= 12.8018274800815|digamma= 1.5061176684318|erf= 0.842700792949715|erfc= 0.157299207050285|erfinv= 0.47693627620447|erfcinv= 0.47693627620447
IMPORT SPECIAL

SUB TestBessel()
  PRINT "besseli="; SPECIAL.BESSELI(0, 1.0)
  PRINT "besselj="; SPECIAL.BESSELJ(0, 1.0)
  PRINT "besselk="; SPECIAL.BESSELK(0, 1.0)
  PRINT "bessely="; SPECIAL.BESSELY(0, 1.0)
END SUB

SUB TestBeta()
  PRINT "beta="; SPECIAL.BETA(2.0, 3.0)
  PRINT "betainc="; SPECIAL.BETAINC(2.0, 3.0, 0.5)
  PRINT "betaincinv="; SPECIAL.BETAINCINV(2.0, 3.0, 0.5)
  PRINT "betaln="; SPECIAL.BETALN(2.0, 3.0)
END SUB

SUB TestGamma()
  PRINT "gamma="; SPECIAL.GAMMA(5.0)
  PRINT "gammainc="; SPECIAL.GAMMAINC(2.0, 3.0)
  PRINT "gammaincc="; SPECIAL.GAMMAINCC(2.0, 3.0)
  PRINT "gammaincinv="; SPECIAL.GAMMAINCINV(2.0, 0.5)
  PRINT "gammainccinv="; SPECIAL.GAMMAINCCINV(2.0, 0.5)
  PRINT "lgamma="; SPECIAL.LGAMMA(10.0)
  PRINT "digamma="; SPECIAL.DIGAMMA(5.0)
END SUB

SUB TestErr()
  PRINT "erf="; SPECIAL.ERF(1.0)
  PRINT "erfc="; SPECIAL.ERFC(1.0)
  PRINT "erfinv="; SPECIAL.ERFINV(0.5)
  PRINT "erfcinv="; SPECIAL.ERFCINV(0.5)
END SUB

SUB Main()
  TestBessel()
  TestBeta()
  TestGamma()
  TestErr()
END SUB
