IMPORT SPECIAL
' ==============================================================================
' FILE: Math/SpecialFunctions.bas
' Namespace SPECIAL — funzioni matematiche speciali (scipy.special).
' ==============================================================================
'   GAMMA/LGAMMA/DIGAMMA       — gamma, log-gamma, digamma
'   BETA/BETALN                — funzione beta e log-beta
'   ERF/ERFC/ERFINV/ERFCINV    — errore e inverse
'   GAMMAINC/GAMMAINCC         — gamma incompleta (regolarizzata)
'   GAMMAINCINV/GAMMAINCCINV   — inverse della gamma incompleta
'   BETAINC/BETAINCINV         — beta incompleta e inversa (x,y in [0,1])
'   BESSELJ/BESSELY/BESSELI/BESSELK — funzioni di Bessel
' ==============================================================================

PRINT "=== Gamma / Digamma ==="
PRINT "GAMMA(5)       = "; SPECIAL.GAMMA(5.0)       ' 24
PRINT "LGAMMA(10)     = "; SPECIAL.LGAMMA(10.0)
PRINT "DIGAMMA(5)     = "; SPECIAL.DIGAMMA(5.0)

PRINT
PRINT "=== Beta ==="
PRINT "BETA(2,3)      = "; SPECIAL.BETA(2.0, 3.0)    ' 1/12
PRINT "BETALN(2,3)    = "; SPECIAL.BETALN(2.0, 3.0)
PRINT "BETAINC(2,3,.5)= "; SPECIAL.BETAINC(2.0, 3.0, 0.5)
PRINT "BETAINCINV     = "; SPECIAL.BETAINCINV(2.0, 3.0, 0.5)

PRINT
PRINT "=== Errore ==="
PRINT "ERF(1)         = "; SPECIAL.ERF(1.0)
PRINT "ERFC(1)        = "; SPECIAL.ERFC(1.0)
PRINT "ERFINV(0.5)    = "; SPECIAL.ERFINV(0.5)
PRINT "ERFCINV(0.5)   = "; SPECIAL.ERFCINV(0.5)

PRINT
PRINT "=== Gamma incompleta ==="
PRINT "GAMMAINC(2,3)  = "; SPECIAL.GAMMAINC(2.0, 3.0)
PRINT "GAMMAINCC(2,3) = "; SPECIAL.GAMMAINCC(2.0, 3.0)
PRINT "GAMMAINCINV    = "; SPECIAL.GAMMAINCINV(2.0, 0.5)
PRINT "GAMMAINCCINV   = "; SPECIAL.GAMMAINCCINV(2.0, 0.5)

PRINT
PRINT "=== Bessel ==="
PRINT "BESSELJ(0,1)   = "; SPECIAL.BESSELJ(0, 1.0)
PRINT "BESSELY(0,1)   = "; SPECIAL.BESSELY(0, 1.0)
PRINT "BESSELI(0,1)   = "; SPECIAL.BESSELI(0, 1.0)
PRINT "BESSELK(0,1)   = "; SPECIAL.BESSELK(0, 1.0)

PRINT
PRINT "Completato."
