' MiniTest: namespace SPECIAL — funzioni matematiche speciali — parita' 4 backend.
' Deterministiche. Copre le 19 funzioni della superficie canonica.
' EXPECT: gamma= 24.0000000000001|lgamma= 12.8018274800815|erf= 0.842700792949715|erfc= 0.157299207050285|erfinv= 0.47693627620447|erfcinv= 0.47693627620447|digamma= 1.5061176684318|betaln=-2.484906649788|beta= 0.0833333333333333|betainc= 0.6875|betaincinv= 0.385727568132389|besselj= 0.765197686557967|bessely= 0.088256964215677|besseli= 1.26606587775201|besselk= 0.421024438240708|gammainc= 0.800851726528544|gammaincc= 0.199148273471456|gammaincinv= 1.67834699001666|gammainccinv= 1.67834699001666
IMPORT SPECIAL

SUB Main
  PRINT "gamma="; SPECIAL.GAMMA(5.0)
  PRINT "lgamma="; SPECIAL.LGAMMA(10.0)
  PRINT "erf="; SPECIAL.ERF(1.0)
  PRINT "erfc="; SPECIAL.ERFC(1.0)
  PRINT "erfinv="; SPECIAL.ERFINV(0.5)
  PRINT "erfcinv="; SPECIAL.ERFCINV(0.5)
  PRINT "digamma="; SPECIAL.DIGAMMA(5.0)
  PRINT "betaln="; SPECIAL.BETALN(2.0, 3.0)
  PRINT "beta="; SPECIAL.BETA(2.0, 3.0)
  PRINT "betainc="; SPECIAL.BETAINC(2.0, 3.0, 0.5)
  PRINT "betaincinv="; SPECIAL.BETAINCINV(2.0, 3.0, 0.5)
  PRINT "besselj="; SPECIAL.BESSELJ(0, 1.0)
  PRINT "bessely="; SPECIAL.BESSELY(0, 1.0)
  PRINT "besseli="; SPECIAL.BESSELI(0, 1.0)
  PRINT "besselk="; SPECIAL.BESSELK(0, 1.0)
  PRINT "gammainc="; SPECIAL.GAMMAINC(2.0, 3.0)
  PRINT "gammaincc="; SPECIAL.GAMMAINCC(2.0, 3.0)
  PRINT "gammaincinv="; SPECIAL.GAMMAINCINV(2.0, 0.5)
  PRINT "gammainccinv="; SPECIAL.GAMMAINCCINV(2.0, 0.5)
END SUB
