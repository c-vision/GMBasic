' SPECIAL — nucleo scipy.special (GAMMA, LGAMMA, BETA, ERF, ERFC, GAMMAINC,
' GAMMAINCC, BETAINC, DIGAMMA, BESSELJ/Y/I/K).
' Oracolo: SciPy 1.18 (scipy.special) — gli output devono coincidere entro 1e-9.
IMPORT SPECIAL

SUB Main()
  PRINT "gamma(5)    ="; SPECIAL.GAMMA(5)
  PRINT "gamma(0.5)  ="; SPECIAL.GAMMA(0.5)
  PRINT "lgamma(5)   ="; SPECIAL.LGAMMA(5)
  PRINT "beta(2,3)   ="; SPECIAL.BETA(2, 3)
  PRINT "erf(0.5)    ="; SPECIAL.ERF(0.5)
  PRINT "erfc(0.5)   ="; SPECIAL.ERFC(0.5)
  PRINT "gammainc    ="; SPECIAL.GAMMAINC(2, 3)
  PRINT "gammaincc   ="; SPECIAL.GAMMAINCC(2, 3)
  PRINT "betainc     ="; SPECIAL.BETAINC(2, 3, 0.5)
  PRINT "digamma(3)  ="; SPECIAL.DIGAMMA(3)
  PRINT "jv(1,2)     ="; SPECIAL.BESSELJ(1, 2)
  PRINT "yv(1,2)     ="; SPECIAL.BESSELY(1, 2)
  PRINT "iv(1,2)     ="; SPECIAL.BESSELI(1, 2)
  PRINT "kv(1,2)     ="; SPECIAL.BESSELK(1, 2)
  PRINT "betaln      ="; SPECIAL.BETALN(2, 3)
  PRINT "erfinv(.5)  ="; SPECIAL.ERFINV(0.5)
  PRINT "erfcinv(.5) ="; SPECIAL.ERFCINV(0.5)
  PRINT "gammaincinv ="; SPECIAL.GAMMAINCINV(2, 0.8)
  PRINT "gammainccinv="; SPECIAL.GAMMAINCCINV(2, 0.2)
  PRINT "betaincinv  ="; SPECIAL.BETAINCINV(2, 3, 0.5)
END SUB
