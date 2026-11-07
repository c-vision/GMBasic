' COMPLEX (FFI) — le 28 funzioni dei numeri complessi su valori verificabili a mano.
' Il numero complesso è un TYPE passato per valore: z = COMPLEX.MAKE(re, im).
' Le chiamate innestate (un risultato usato subito come argomento) sono spezzate in
' variabili temporanee: è lo stile portabile su tutte e quattro le gambe.
' Le identità (Eulero, radici, inverse delle trigonometriche) sono verificate con EQ.
' EXPECT: abs=5.000000 arg=0.927295||norm2=25.0||eq= 1   0|add=4.0,6.0||sub=-2.0,-2.0||mul=-5.0,10.0||div=0.440000,0.080000||neg=-1.0,-2.0||conj=1.0,-2.0||scale=2.0,4.0|euler= 1||log-1=0.000000,3.141593||sqrt-1=0.000000,1.000000||i2= 1||sqrt2= 1|sin-i=0.000000,1.175201||cos-i=1.543081,0.000000||tan-i=0.000000,0.761594|sinh-i=0.000000,0.841471||cosh-i=0.540302,0.000000||tanh-i=0.000000,1.557408|asin=0.523599,0.000000||acos=1.047198,-0.000000||atan=0.785398,0.000000||asinh=0.881374,0.000000||acosh=1.316958,0.000000||atanh=0.549306,0.000000|rt-trig= 1   1   1|rt-ip= 1   1   1|div0=0.0,0.0||pow0=0.0,0.0||sqrt0=0.0,0.0||log0-inf=-1||f=0
IMPORT COMPLEX

CONST PIVAL = 3.14159265358979323846

SUB Main()
  DIM a AS Complex, b AS Complex, z AS Complex, t AS Complex

  ' ── costruzione e misure: 3 + 4i ──────────────────────────────────────────
  a = COMPLEX.MAKE(3, 4)
  PRINT "abs="; FORMAT(COMPLEX.CABS(a), "#.000000"); " arg="; FORMAT(COMPLEX.ARG(a), "#.000000")
  PRINT "|norm2="; FORMAT(COMPLEX.NORM2(a), "#.0")
  b = COMPLEX.MAKE(3, 4)
  PRINT "|eq="; COMPLEX.EQ(a, b, 0.000001); " ";
  b = COMPLEX.MAKE(3, 4.1)
  PRINT COMPLEX.EQ(a, b, 0.000001)

  ' ── aritmetica ────────────────────────────────────────────────────────────
  a = COMPLEX.MAKE(1, 2)
  b = COMPLEX.MAKE(3, 4)
  z = COMPLEX.ADD(a, b)
  PRINT "add="; FORMAT(z.RE, "#.0"); ","; FORMAT(z.IM, "#.0")
  z = COMPLEX.SUB(a, b)
  PRINT "|sub="; FORMAT(z.RE, "#.0"); ","; FORMAT(z.IM, "#.0")
  z = COMPLEX.MUL(a, b)
  PRINT "|mul="; FORMAT(z.RE, "#.0"); ","; FORMAT(z.IM, "#.0")
  z = COMPLEX.DIV(a, b)
  PRINT "|div="; FORMAT(z.RE, "#.000000"); ","; FORMAT(z.IM, "#.000000")
  z = COMPLEX.NEG(a)
  PRINT "|neg="; FORMAT(z.RE, "#.0"); ","; FORMAT(z.IM, "#.0")
  z = COMPLEX.CONJ(a)
  PRINT "|conj="; FORMAT(z.RE, "#.0"); ","; FORMAT(z.IM, "#.0")
  z = COMPLEX.SCALE(a, 2)
  PRINT "|scale="; FORMAT(z.RE, "#.0"); ","; FORMAT(z.IM, "#.0")

  ' ── esponenziale, logaritmo, potenza ──────────────────────────────────────
  ' e^(i·pi) = -1
  t = COMPLEX.MAKE(0, PIVAL)
  z = COMPLEX.CEXP(t)
  t = COMPLEX.MAKE(-1, 0)
  PRINT "euler="; COMPLEX.EQ(z, t, 0.000001)
  ' log(-1) = i·pi
  z = COMPLEX.CLOG(t)
  PRINT "|log-1="; FORMAT(z.RE, "#.000000"); ","; FORMAT(z.IM, "#.000000")
  ' sqrt(-1) = i
  z = COMPLEX.CSQRT(t)
  PRINT "|sqrt-1="; FORMAT(z.RE, "#.000000"); ","; FORMAT(z.IM, "#.000000")
  ' i^2 = -1
  a = COMPLEX.MAKE(0, 1)
  b = COMPLEX.MAKE(2, 0)
  z = COMPLEX.POW(a, b)
  t = COMPLEX.MAKE(-1, 0)
  PRINT "|i2="; COMPLEX.EQ(z, t, 0.000001)
  ' radice quadrata: sqrt(z)^2 = z
  a = COMPLEX.MAKE(3, 4)
  z = COMPLEX.CSQRT(a)
  t = COMPLEX.MUL(z, z)
  PRINT "|sqrt2="; COMPLEX.EQ(t, a, 0.000001)

  ' ── trigonometriche: valori esatti in i·teta ──────────────────────────────
  ' sin(i) = i·sinh(1); cos(i) = cosh(1); tan(i) = i·tanh(1)
  a = COMPLEX.MAKE(0, 1)
  z = COMPLEX.CSIN(a)
  PRINT "sin-i="; FORMAT(z.RE, "#.000000"); ","; FORMAT(z.IM, "#.000000")
  z = COMPLEX.CCOS(a)
  PRINT "|cos-i="; FORMAT(z.RE, "#.000000"); ","; FORMAT(z.IM, "#.000000")
  z = COMPLEX.CTAN(a)
  PRINT "|tan-i="; FORMAT(z.RE, "#.000000"); ","; FORMAT(z.IM, "#.000000")

  ' ── iperboliche: sinh(i) = i·sin(1); cosh(i) = cos(1); tanh(i) = i·tan(1) ─
  z = COMPLEX.CSINH(a)
  PRINT "sinh-i="; FORMAT(z.RE, "#.000000"); ","; FORMAT(z.IM, "#.000000")
  z = COMPLEX.CCOSH(a)
  PRINT "|cosh-i="; FORMAT(z.RE, "#.000000"); ","; FORMAT(z.IM, "#.000000")
  z = COMPLEX.CTANH(a)
  PRINT "|tanh-i="; FORMAT(z.RE, "#.000000"); ","; FORMAT(z.IM, "#.000000")

  ' ── inverse delle trigonometriche e iperboliche (round-trip) ──────────────
  ' asin(0.5) = pi/6; acos(0.5) = pi/3; atan(1) = pi/4
  a = COMPLEX.MAKE(0.5, 0)
  z = COMPLEX.CASIN(a)
  PRINT "asin="; FORMAT(z.RE, "#.000000"); ","; FORMAT(z.IM, "#.000000")
  z = COMPLEX.CACOS(a)
  PRINT "|acos="; FORMAT(z.RE, "#.000000"); ","; FORMAT(z.IM, "#.000000")
  a = COMPLEX.MAKE(1, 0)
  z = COMPLEX.CATAN(a)
  PRINT "|atan="; FORMAT(z.RE, "#.000000"); ","; FORMAT(z.IM, "#.000000")
  ' asinh(1), acosh(2), atanh(0.5)
  z = COMPLEX.ASINH(a)
  PRINT "|asinh="; FORMAT(z.RE, "#.000000"); ","; FORMAT(z.IM, "#.000000")
  a = COMPLEX.MAKE(2, 0)
  z = COMPLEX.ACOSH(a)
  PRINT "|acosh="; FORMAT(z.RE, "#.000000"); ","; FORMAT(z.IM, "#.000000")
  a = COMPLEX.MAKE(0.5, 0)
  z = COMPLEX.ATANH(a)
  PRINT "|atanh="; FORMAT(z.RE, "#.000000"); ","; FORMAT(z.IM, "#.000000")

  ' round-trip: sin(asin w) = w, cos(acos w) = w, tan(atan w) = w
  b = COMPLEX.MAKE(0.25, 0.75)
  PRINT "rt-trig=";
  z = COMPLEX.CASIN(b)
  t = COMPLEX.CSIN(z)
  PRINT COMPLEX.EQ(t, b, 0.000001);
  z = COMPLEX.CACOS(b)
  t = COMPLEX.CCOS(z)
  PRINT " "; COMPLEX.EQ(t, b, 0.000001);
  z = COMPLEX.CATAN(b)
  t = COMPLEX.CTAN(z)
  PRINT " "; COMPLEX.EQ(t, b, 0.000001)
  ' round-trip: sinh(asinh w) = w, cosh(acosh w) = w, tanh(atanh w) = w
  b = COMPLEX.MAKE(1.5, 0.5)
  PRINT "rt-ip=";
  z = COMPLEX.ASINH(b)
  t = COMPLEX.CSINH(z)
  PRINT COMPLEX.EQ(t, b, 0.000001);
  z = COMPLEX.ACOSH(b)
  t = COMPLEX.CCOSH(z)
  PRINT " "; COMPLEX.EQ(t, b, 0.000001);
  z = COMPLEX.ATANH(b)
  t = COMPLEX.CTANH(z)
  PRINT " "; COMPLEX.EQ(t, b, 0.000001)

  ' ── casi limite documentati ───────────────────────────────────────────────
  a = COMPLEX.MAKE(1, 2)
  b = COMPLEX.MAKE(0, 0)
  z = COMPLEX.DIV(a, b)
  PRINT "div0="; FORMAT(z.RE, "#.0"); ","; FORMAT(z.IM, "#.0")
  a = COMPLEX.MAKE(0, 0)
  b = COMPLEX.MAKE(3, 0)
  z = COMPLEX.POW(a, b)
  PRINT "|pow0="; FORMAT(z.RE, "#.0"); ","; FORMAT(z.IM, "#.0")
  z = COMPLEX.CSQRT(a)
  PRINT "|sqrt0="; FORMAT(z.RE, "#.0"); ","; FORMAT(z.IM, "#.0")
  z = COMPLEX.CLOG(a)
  PRINT "|log0-inf="; COMPLEX.CABS(z) > 1E300
  PRINT "|f=0"
END SUB
