' FILE: Special/SpecialEllipticBessel.bas
' SCENARIO DIDATTICO — due fenomeni dove servono le funzioni speciali appena esposte:
' il pendolo non lineare (integrali e funzioni ellittiche) e lo scattering con le
' Bessel sferiche.
'
'   pendolo: L = 1 m, g = 9.81, ampiezza 60 gradi (k = sin(30) = 0.5)
'     periodo esatto T = 4 sqrt(L/g) K(k)          (k = sin(theta0/2))
'     moto   theta(t) = 2 asin(k sn(K - w t, k))   con w = sqrt(g/L)
'     periodo approssimato (piccole oscillazioni) T0 = 2 pi sqrt(L/g)
'   Bessel sferiche: j0(x) = sin(x)/x, j1(x) = sin(x)/x^2 - cos(x)/x
'
' Domini: SPECIAL. Output deterministico.
IMPORT SPECIAL

CONST PIVAL = 3.14159265358979323846
CONST GRAV = 9.81
CONST LUN = 1

SUB Main()
  DIM k AS DOUBLE, kk AS DOUBLE, T AS DOUBLE, T0 AS DOUBLE
  DIM w AS DOUBLE, theta AS DOUBLE
  DIM phi AS DOUBLE, e1 AS DOUBLE, e2 AS DOUBLE

  ' ══════════════════════════════════════════════════════════════════════════
  ' A) PENDOLO NON LINEARE
  ' ══════════════════════════════════════════════════════════════════════════
  k = MATH.SIN(60 * PIVAL / 180 / 2)          ' 60 gradi di ampiezza -> k = 0.5
  kk = SPECIAL.ELLIPTICK(k)
  w = MATH.SQR(GRAV / LUN)
  T = 4 * MATH.SQR(LUN / GRAV) * kk
  T0 = 2 * PIVAL * MATH.SQR(LUN / GRAV)
  PRINT "A) pendolo: L=1 m, g=9.81, ampiezza 60 gradi, k = "; FORMAT(k, "#.000000")
  PRINT " K(k) = "; FORMAT(kk, "#.000000"); "  T esatto = "; FORMAT(T, "#.000000000"); " s"
  PRINT " T approssimato (piccole oscillazioni) = "; FORMAT(T0, "#.000000000"); " s"
  PRINT " allungamento = "; FORMAT((T - T0) / T0 * 100, "#.000000"); " %"

  ' moto: a t=0 il pendolo è all'ampiezza massima (sn(K,k) = 1)
  PRINT " moto theta(t) = 2 asin(k sn(K - w t, k)):"
  FOR t = 0 TO 2
    e1 = SPECIAL.JACOBISN(kk - w * t, k)
    theta = 2 * MATH.ASIN(k * e1)
    PRINT "  t="; t; " s  theta = "; FORMAT(theta * 180 / PIVAL, "#.000000"); " gradi"
  NEXT t
  ' ampiezza e funzioni ausiliarie a metà periodo (argomento K/2)
  PRINT " K/2: am = "; FORMAT(SPECIAL.JACOBIAM(kk / 2, k), "#.000000");
  PRINT "  cn = "; FORMAT(SPECIAL.JACOBICN(kk / 2, k), "#.000000");
  PRINT "  dn = "; FORMAT(SPECIAL.JACOBIDN(kk / 2, k), "#.000000")

  ' integrale incompleto: F(phi,k) ed E(phi,k) a phi = pi/4
  phi = PIVAL / 4
  PRINT " F(pi/4, k) = "; FORMAT(SPECIAL.ELLIPTICF(phi, k), "#.000000");
  PRINT "  E(pi/4, k) = "; FORMAT(SPECIAL.ELLIPTICEINC(phi, k), "#.000000")

  ' identità con Carlson: E(k) = RF(0,1-k^2,1) - k^2/3 RD(0,1-k^2,1)
  e1 = SPECIAL.ELLIPTICRF(0, 1 - k * k, 1)
  e2 = SPECIAL.ELLIPTICRD(0, 1 - k * k, 1)
  PRINT " E(k) da RF/RD = "; FORMAT(e1 - k * k / 3 * e2, "#.000000");
  PRINT "  E(k) diretto = "; FORMAT(SPECIAL.ELLIPTICE(k), "#.000000")
  ' nome q(k) della serie di Fourier
  PRINT " nome q(k) = "; FORMAT(SPECIAL.ELLIPTICNOME(k), "#.000000000000")

    ' ══════════════════════════════════════════════════════════════════════════
  ' B) BESSEL SFERICHE: scattering a x = 2
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "B) Bessel sferiche in x = 2"
  ' j0(2) = sin(2)/2 = 0.454649, j1(2) = sin(2)/4 - cos(2)/2 = 0.435398
  PRINT " j0(2) = "; FORMAT(SPECIAL.BESSELSPHJ(0, 2), "#.000000000000");
  PRINT "  j1(2) = "; FORMAT(SPECIAL.BESSELSPHJ(1, 2), "#.000000000000")
  ' y0(2) = -cos(2)/2 = 0.208073, y1(2) = -cos(2)/4 - sin(2)/2 = -0.350612
  PRINT " y0(2) = "; FORMAT(SPECIAL.BESSELSPHY(0, 2), "#.000000000000");
  PRINT "  y1(2) = "; FORMAT(SPECIAL.BESSELSPHY(1, 2), "#.000000000000")
  PRINT " Rayleigh (j1/x) = "; FORMAT(SPECIAL.BESSELSPHJ(1, 2) / 2, "#.000000000000")
  PRINT "fine"
END SUB
