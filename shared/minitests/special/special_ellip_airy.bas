' SPECIAL (FFI) — integrali/funzioni ellittiche e Bessel sferiche.
' Valori esatti verificabili: Bessel sferiche in 1
' (sin/cos), identità ellittiche (F(pi/2,k)=K(k), sn^2+cn^2=1, dn^2+k^2 sn^2=1).
' EXPECT: |j0=0.841470984808 j1=0.301168678940||y0=-0.540302305868 y1=-1.381773290676||k0=1.570796326795,1.570796326795||k05=1.685750,e=1.467462||fid=0.000000000000||eid=0.000000000000||rf=1.685750354813 K=1.685750354813||rf1=1.000000000000 rd1=1.000000000000||jac=0.475083,0.879941,0.971377||jac1=0.000000000000 jac2=0.000000000000||u0=0.000000,0.000000,1.000000,1.000000||nome=0.017972387009||f=0
IMPORT SPECIAL

CONST PIVAL = 3.14159265358979323846

SUB Main()
  DIM k AS DOUBLE, phi AS DOUBLE, e1 AS DOUBLE, e2 AS DOUBLE, e3 AS DOUBLE

  ' ── Bessel sferiche in x = 1 ──────────────────────────────────────────────
  ' j0(1) = sin(1), y0(1) = -cos(1), j1(1) = sin(1) - cos(1)
  PRINT "|j0="; FORMAT(SPECIAL.BESSELSPHJ(0, 1), "#.000000000000"); " j1="; FORMAT(SPECIAL.BESSELSPHJ(1, 1), "#.000000000000")
  PRINT "|y0="; FORMAT(SPECIAL.BESSELSPHY(0, 1), "#.000000000000"); " y1="; FORMAT(SPECIAL.BESSELSPHY(1, 1), "#.000000000000")

  ' ── integrali ellittici ───────────────────────────────────────────────────
  ' K(0) = E(0) = pi/2
  PRINT "|k0="; FORMAT(SPECIAL.ELLIPTICK(0), "#.000000000000"); ","; FORMAT(SPECIAL.ELLIPTICE(0), "#.000000000000")
  ' K(0.5) = 1.686271, E(0.5) = 1.467462 (tavole)
  PRINT "|k05="; FORMAT(SPECIAL.ELLIPTICK(0.5), "#.000000"); ",e="; FORMAT(SPECIAL.ELLIPTICE(0.5), "#.000000")
  ' F(pi/2, k) = K(k)  e  Einc(pi/2, k) = E(k)  (identità)
  k = 0.5
  phi = PIVAL / 2
  e1 = SPECIAL.ELLIPTICF(phi, k)
  e2 = SPECIAL.ELLIPTICK(k)
  PRINT "|fid="; FORMAT(MATH.ABS(e1 - e2), "#.000000000000")
  e1 = SPECIAL.ELLIPTICEINC(phi, k)
  PRINT "|eid="; FORMAT(MATH.ABS(e1 - SPECIAL.ELLIPTICE(k)), "#.000000000000")
  ' K(k) = RF(0, 1-k^2, 1)
  PRINT "|rf="; FORMAT(SPECIAL.ELLIPTICRF(0, 1 - k * k, 1), "#.000000000000"); " K="; FORMAT(SPECIAL.ELLIPTICK(k), "#.000000000000")
  ' RD(1,1,1) = 1? verificato con la definizione: RF(1,1,1) = 1
  PRINT "|rf1="; FORMAT(SPECIAL.ELLIPTICRF(1, 1, 1), "#.000000000000"); " rd1="; FORMAT(SPECIAL.ELLIPTICRD(1, 1, 1), "#.000000000000")

  ' ── Jacobi: identità sn^2 + cn^2 = 1 e dn^2 + k^2 sn^2 = 1 ────────────────
  e1 = SPECIAL.JACOBISN(0.5, k)
  e2 = SPECIAL.JACOBICN(0.5, k)
  e3 = SPECIAL.JACOBIDN(0.5, k)
  PRINT "|jac="; FORMAT(e1, "#.000000"); ","; FORMAT(e2, "#.000000"); ","; FORMAT(e3, "#.000000")
  PRINT "|jac1="; FORMAT(MATH.ABS(e1 * e1 + e2 * e2 - 1), "#.000000000000"); " jac2="; FORMAT(MATH.ABS(e3 * e3 + k * k * e1 * e1 - 1), "#.000000000000")
  ' am(0,k)=0, sn(0,k)=0, cn(0,k)=1, dn(0,k)=1
  PRINT "|u0="; FORMAT(SPECIAL.JACOBIAM(0, k), "#.000000"); ","; FORMAT(SPECIAL.JACOBISN(0, k), "#.000000"); ","; FORMAT(SPECIAL.JACOBICN(0, k), "#.000000"); ","; FORMAT(SPECIAL.JACOBIDN(0, k), "#.000000")
  ' nome q(k) (piccolo per k=0.5)
  PRINT "|nome="; FORMAT(SPECIAL.ELLIPTICNOME(0.5), "#.000000000000")
  PRINT "|f=0"
END SUB
