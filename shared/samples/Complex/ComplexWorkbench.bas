' FILE: Complex/ComplexWorkbench.bas
' SCENARIO DIDATTICO — quattro postazioni di un laboratorio sui numeri complessi,
' con numeri verificabili a mano. Ogni postazione usa le funzioni che le servono:
'
'   A) circuito RL-C serie a 50 Hz: impedenza, corrente, potenza (ADD, MUL, DIV,
'      NEG, CONJ, SCALE, CABS, ARG, NORM2, EQ)
'   B) radici di z^2 + 2z + 5: discriminante negativo e radici complesse
'      (CSQRT, ADD, SUB, NEG, SCALE, DIV)
'   C) insieme di Mandelbrot: iterazione z <- z^2 + c, conteggio delle fughe
'      (MUL, ADD, NORM2)
'   D) identità: Eulero, logaritmo, potenza e le inverse delle trigonometriche
'      (CEXP, CLOG, POW, CSIN, CCOS, CTAN, CSINH, CCOSH, CTANH, CASIN, CACOS,
'      CATAN, ASINH, ACOSH, ATANH, MAKE)
'
' Il numero complesso è il TYPE `Complex` passato per valore. Ogni risultato che
' serve come argomento passa da una variabile: è lo stile portabile su tutte e
' quattro le gambe (le chiamate innestate sono un caso che i backend trattano
' diversamente).
' Domini: COMPLEX. Output deterministico (le identità si verificano con EQ).
IMPORT COMPLEX

CONST PIVAL = 3.14159265358979323846

SUB Main()
  DIM z AS Complex, t AS Complex, u AS Complex, w2 AS Complex
  DIM f AS DOUBLE, omega AS DOUBLE
  DIM n AS LONG, inside AS LONG

  ' ══════════════════════════════════════════════════════════════════════════
  ' A) CIRCUITO RL-C SERIE — R = 100 ohm, L = 0.5 H, C = 10 uF, V = 230 V
  ' ══════════════════════════════════════════════════════════════════════════
  f = 50
  omega = 2 * PIVAL * f
  PRINT "A) circuito RL-C a "; f; " Hz, omega = "; FORMAT(omega, "#.000")

  ' reattanze: Xl = wL (induttiva, +j), Xc = 1/(wC) (capacitiva, -j)
  z = COMPLEX.MAKE(0, omega * 0.5)
  PRINT " Xl = "; FORMAT(COMPLEX.ARG(z), "#.000"); " rad, |Xl| = "; FORMAT(COMPLEX.CABS(z), "#.000")
  t = COMPLEX.MAKE(0, omega * 0.00001)          ' j*w*C
  u = COMPLEX.MAKE(1, 0)
  z = COMPLEX.DIV(u, t)                          ' Zc = 1/(j*w*C)
  PRINT " Xc = "; FORMAT(COMPLEX.ARG(z), "#.000"); " rad, |Xc| = "; FORMAT(COMPLEX.CABS(z), "#.000")

  ' Z = R + jXl + Zc
  t = COMPLEX.MAKE(0, omega * 0.5)               ' jXl di nuovo (z ora è Zc)
  u = COMPLEX.MAKE(100, 0)
  w2 = COMPLEX.ADD(u, t)
  z = COMPLEX.ADD(w2, z)
  PRINT " Z = "; FORMAT(z.RE, "#.00"); " + j("; FORMAT(z.IM, "#.00"); ") ohm"
  PRINT " |Z| = "; FORMAT(COMPLEX.CABS(z), "#.0000"); " fase = "; FORMAT(COMPLEX.ARG(z), "#.000000"); " rad"
  PRINT " |Z|^2 = "; FORMAT(COMPLEX.NORM2(z), "#.00")
  ' l'opposto dell'impedenza
  u = COMPLEX.NEG(z)
  PRINT " -Z = "; FORMAT(u.RE, "#.00"); " + j("; FORMAT(u.IM, "#.00"); ") ohm"

  ' I = V / Z  (V = 230 + j0)
  u = COMPLEX.MAKE(230, 0)
  t = COMPLEX.DIV(u, z)
  PRINT " I = "; FORMAT(t.RE, "#.0000"); " + j("; FORMAT(t.IM, "#.0000"); ") A"
  PRINT " |I| = "; FORMAT(COMPLEX.CABS(t), "#.0000"); " fase = "; FORMAT(COMPLEX.ARG(t), "#.000000"); " rad"

  ' S = V * conj(I): potenza complessa; P = re(S), Q = im(S)
  u = COMPLEX.CONJ(t)
  w2 = COMPLEX.MAKE(230, 0)
  z = COMPLEX.MUL(w2, u)
  PRINT " P = "; FORMAT(z.RE, "#.00"); " W, Q = "; FORMAT(z.IM, "#.00"); " var"

  ' la potenza attiva è anche |I|^2 * R
  u = COMPLEX.MAKE(1, 0)
  f = COMPLEX.NORM2(t) * 100
  w2 = COMPLEX.SCALE(u, f)
  u = COMPLEX.MAKE(z.RE, 0)
  PRINT " P da |I|^2*R = "; FORMAT(w2.RE, "#.00"); " W, uguale = "; COMPLEX.EQ(w2, u, 0.01)

  ' ══════════════════════════════════════════════════════════════════════════
  ' B) RADICI DI z^2 + 2z + 5 = 0  →  z = -1 ± 2i
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "B) z^2 + 2z + 5 = 0"
  ' discriminante: b^2 - 4ac = 4 - 20 = -16
  t = COMPLEX.MAKE(4, 0)
  u = COMPLEX.MAKE(20, 0)
  z = COMPLEX.SUB(t, u)
  PRINT " discriminante = "; FORMAT(z.RE, "#.0")
  z = COMPLEX.CSQRT(z)
  PRINT " sqrt(disc) = "; FORMAT(z.RE, "#.0"); " + j("; FORMAT(z.IM, "#.0"); ")"
  u = COMPLEX.MAKE(2, 0)
  t = COMPLEX.NEG(u)                             ' -b
  w2 = COMPLEX.SUB(t, z)                         ' -b - sqrt(d)
  t = COMPLEX.DIV(w2, u)                         ' z1
  PRINT " z1 = "; FORMAT(t.RE, "#.0"); " + j("; FORMAT(t.IM, "#.0"); ")"
  z = COMPLEX.MUL(t, t)                          ' z1^2
  u = COMPLEX.SCALE(t, 2)                        ' 2*z1
  z = COMPLEX.ADD(z, u)
  w2 = COMPLEX.MAKE(5, 0)
  z = COMPLEX.ADD(z, w2)                         ' z1^2 + 2*z1 + 5
  w2 = COMPLEX.MAKE(0, 0)
  PRINT " residuo = "; FORMAT(COMPLEX.CABS(z), "#.000000"); " (zero = "; COMPLEX.EQ(z, w2, 0.000001); ")"

  ' ══════════════════════════════════════════════════════════════════════════
  ' C) MANDELBROT — quante iterazioni z <- z^2 + c restano dentro |z| < 2
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "C) Mandelbrot, 30 iterazioni"
  t = COMPLEX.MAKE(-0.5, 0.5)                    ' c
  z = COMPLEX.MAKE(0, 0)
  n = 0 : inside = 1
  DO
    n = n + 1
    u = COMPLEX.MUL(z, z)
    z = COMPLEX.ADD(u, t)
    IF COMPLEX.NORM2(z) > 4 THEN
      inside = 0
      EXIT DO
    END IF
  LOOP WHILE n < 30
  PRINT " c = -0.5+0.5i: iterazioni = "; n; ", dentro = "; inside
  PRINT " |z| al termine = "; FORMAT(COMPLEX.CABS(z), "#.0000")
  ' un punto che fugge: c = 0.5+0.5i
  t = COMPLEX.MAKE(0.5, 0.5)
  z = COMPLEX.MAKE(0, 0)
  n = 0 : inside = 1
  DO
    n = n + 1
    u = COMPLEX.MUL(z, z)
    z = COMPLEX.ADD(u, t)
    IF COMPLEX.NORM2(z) > 4 THEN
      inside = 0
      EXIT DO
    END IF
  LOOP WHILE n < 30
  PRINT " c = 0.5+0.5i: iterazioni = "; n; ", dentro = "; inside
  ' un punto interno (c = 0) non fugge mai
  t = COMPLEX.MAKE(0, 0)
  z = COMPLEX.MAKE(0, 0)
  n = 0 : inside = 1
  DO
    n = n + 1
    u = COMPLEX.MUL(z, z)
    z = COMPLEX.ADD(u, t)
    IF COMPLEX.NORM2(z) > 4 THEN
      inside = 0
      EXIT DO
    END IF
  LOOP WHILE n < 30
  PRINT " c = 0: iterazioni = "; n; ", dentro = "; inside

  ' ══════════════════════════════════════════════════════════════════════════
  ' D) IDENTITÀ — esponenziale, logaritmo, potenza, trigonometriche e inverse
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "D) identita"
  ' e^(i*pi) = -1
  t = COMPLEX.MAKE(0, PIVAL)
  z = COMPLEX.CEXP(t)
  u = COMPLEX.MAKE(-1, 0)
  PRINT " e^(i pi) = "; FORMAT(z.RE, "#.000000"); " + j("; FORMAT(z.IM, "#.000000"); ")"
  ' log(-1) = i*pi
  z = COMPLEX.CLOG(u)
  PRINT " log(-1) = "; FORMAT(z.RE, "#.000000"); " + j("; FORMAT(z.IM, "#.000000"); ")"
  ' (1+i)^2 = 2i
  t = COMPLEX.MAKE(1, 1)
  u = COMPLEX.MAKE(2, 0)
  z = COMPLEX.POW(t, u)
  PRINT " (1+i)^2 = "; FORMAT(z.RE, "#.000000"); " + j("; FORMAT(z.IM, "#.000000"); ")"
  ' sqrt(z)^2 = z  per z = 3+4i
  t = COMPLEX.MAKE(3, 4)
  z = COMPLEX.CSQRT(t)
  u = COMPLEX.MUL(z, z)
  PRINT " sqrt(3+4i)^2 = z: "; COMPLEX.EQ(u, t, 0.000001)

  ' sin(i) = i*sinh(1), cos(i) = cosh(1), tan(i) = i*tanh(1)
  u = COMPLEX.MAKE(0, 1)
  z = COMPLEX.CSIN(u)
  PRINT " sin(i) = "; FORMAT(z.RE, "#.000000"); " + j("; FORMAT(z.IM, "#.000000"); ")"
  z = COMPLEX.CCOS(u)
  PRINT " cos(i) = "; FORMAT(z.RE, "#.000000"); " + j("; FORMAT(z.IM, "#.000000"); ")"
  z = COMPLEX.CTAN(u)
  PRINT " tan(i) = "; FORMAT(z.RE, "#.000000"); " + j("; FORMAT(z.IM, "#.000000"); ")"
  ' sinh(i) = i*sin(1), cosh(i) = cos(1), tanh(i) = i*tan(1)
  z = COMPLEX.CSINH(u)
  PRINT " sinh(i) = "; FORMAT(z.RE, "#.000000"); " + j("; FORMAT(z.IM, "#.000000"); ")"
  z = COMPLEX.CCOSH(u)
  PRINT " cosh(i) = "; FORMAT(z.RE, "#.000000"); " + j("; FORMAT(z.IM, "#.000000"); ")"
  z = COMPLEX.CTANH(u)
  PRINT " tanh(i) = "; FORMAT(z.RE, "#.000000"); " + j("; FORMAT(z.IM, "#.000000"); ")"

  ' inverse reali: asin(0.5)=pi/6, acos(0.5)=pi/3, atan(1)=pi/4
  t = COMPLEX.MAKE(0.5, 0)
  z = COMPLEX.CASIN(t)
  PRINT " asin(0.5) = "; FORMAT(z.RE, "#.000000")
  z = COMPLEX.CACOS(t)
  PRINT " acos(0.5) = "; FORMAT(z.RE, "#.000000")
  t = COMPLEX.MAKE(1, 0)
  z = COMPLEX.CATAN(t)
  PRINT " atan(1)   = "; FORMAT(z.RE, "#.000000")
  z = COMPLEX.ASINH(t)
  PRINT " asinh(1)  = "; FORMAT(z.RE, "#.000000")
  t = COMPLEX.MAKE(2, 0)
  z = COMPLEX.ACOSH(t)
  PRINT " acosh(2)  = "; FORMAT(z.RE, "#.000000")
  t = COMPLEX.MAKE(0.5, 0)
  z = COMPLEX.ATANH(t)
  PRINT " atanh(0.5)= "; FORMAT(z.RE, "#.000000")

  ' round-trip su un valore complesso: sin(asin w) = w ecc.
  t = COMPLEX.MAKE(0.25, 0.75)
  PRINT " round-trip trig: ";
  z = COMPLEX.CASIN(t)
  u = COMPLEX.CSIN(z)
  PRINT COMPLEX.EQ(u, t, 0.000001);
  z = COMPLEX.CACOS(t)
  u = COMPLEX.CCOS(z)
  PRINT " "; COMPLEX.EQ(u, t, 0.000001);
  z = COMPLEX.CATAN(t)
  u = COMPLEX.CTAN(z)
  PRINT " "; COMPLEX.EQ(u, t, 0.000001)
  t = COMPLEX.MAKE(1.5, 0.5)
  PRINT " round-trip iper: ";
  z = COMPLEX.ASINH(t)
  u = COMPLEX.CSINH(z)
  PRINT COMPLEX.EQ(u, t, 0.000001);
  z = COMPLEX.ACOSH(t)
  u = COMPLEX.CCOSH(z)
  PRINT " "; COMPLEX.EQ(u, t, 0.000001);
  z = COMPLEX.ATANH(t)
  u = COMPLEX.CTANH(z)
  PRINT " "; COMPLEX.EQ(u, t, 0.000001)

  ' casi limite documentati
  t = COMPLEX.MAKE(1, 2)
  u = COMPLEX.MAKE(0, 0)
  z = COMPLEX.DIV(t, u)
  PRINT " div per zero = "; FORMAT(COMPLEX.CABS(z), "#.0"); " (nessun errore, risultato nullo)"
  z = COMPLEX.CSQRT(u)
  PRINT " sqrt(0) = "; FORMAT(COMPLEX.CABS(z), "#.0")
  PRINT "fine"
END SUB
