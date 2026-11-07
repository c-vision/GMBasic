' FILE: VecCalc/VecCalcLab.bas
' SCENARIO DIDATTICO — calcolo vettoriale su campi con derivate note:
'
'   A) campo elettrostatico di una carica puntiforme: E(r) = r/|r|^3 e potenziale
'      V = 1/|r|.  grad V = -E, div E = 0 (solenoidale fuori dalla carica),
'      curl E = 0 (irrotazionale: campo conservativo), laplaciano(1/r) = 0
'   B) rotazione rigida G = (-y, x, 0): solenoidale (div = 0) ma NON irrotazionale
'      (curl = (0,0,2))
'   C) elica r(t) = (cos t, sin t, t/5): lunghezza d'arco e triedro di Frenet
'      (kappa = 1/(1+a^2) = 0.961538, tau = a/(1+a^2) = 0.192308 con a = 1/5)
'   D) integrale di linea del campo G lungo il cerchio unitario: lavoro = 2*pi
'   E) integrale di linea scalare di V = 1/r lungo lo stesso cerchio: 2*pi
'   F) flusso di (0,0,1) attraverso il paraboloide z = x^2+y^2 sul quadrato unitario: 1
'
' Domini: VECCALC. Output deterministico.
IMPORT VECCALC

CONST PIVAL = 3.14159265358979323846
CONST H = 0.0001

' Potenziale elettrostatico V = 1/r.
FUNCTION Pot(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS DOUBLE
  RETURN 1 / MATH.SQR(x * x + y * y + z * z)
END FUNCTION

' Campo elettrico E = r/|r|^3.
FUNCTION Elec(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(2) AS DOUBLE
  DIM d AS DOUBLE
  d = (x * x + y * y + z * z)
  d = d * MATH.SQR(d)
  r(0) = x / d
  r(1) = y / d
  r(2) = z / d
  RETURN r
END FUNCTION

' Rotazione rigida G = (-y, x, 0).
FUNCTION Spin(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(2) AS DOUBLE
  r(0) = -y
  r(1) = x
  r(2) = 0
  RETURN r
END FUNCTION

' Cerchio unitario nel piano z = 0.
FUNCTION Ring(t AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(2) AS DOUBLE
  r(0) = MATH.COS(t)
  r(1) = MATH.SIN(t)
  r(2) = 0
  RETURN r
END FUNCTION

' Elica di passo 1/5.
FUNCTION Elix(t AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(2) AS DOUBLE
  r(0) = MATH.COS(t)
  r(1) = MATH.SIN(t)
  r(2) = t / 5
  RETURN r
END FUNCTION

' Paraboloide z = x^2 + y^2 sul quadrato [0,1]x[0,1].
FUNCTION Bowl(u AS DOUBLE, v AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(2) AS DOUBLE
  r(0) = u
  r(1) = v
  r(2) = u * u + v * v
  RETURN r
END FUNCTION

' Campo verticale unitario (per il flusso).
FUNCTION Up(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(2) AS DOUBLE
  r(0) = 0
  r(1) = 0
  r(2) = 1
  RETURN r
END FUNCTION

SUB Main()
  DIM o(10) AS DOUBLE

  ' ══════════════════════════════════════════════════════════════════════════
  ' A) CAMPO ELETTROSTATICO
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "A) carica puntiforme al centro, punto (1,1,1), |r| = "; FORMAT(MATH.SQR(3), "#.000000")
  PRINT " grad V = "; VECCALC.GRADIENT(Pot, 1, 1, 1, H, o)
  PRINT " ("; FORMAT(o(0), "#.000000"); ","; FORMAT(o(1), "#.000000"); ","; FORMAT(o(2), "#.000000"); ")  atteso -E = -0.192450"
  ' E per confronto: stessa grandezza, segno opposto
  PRINT " div E = "; FORMAT(VECCALC.DIVERGENCE(Elec, 1, 1, 1, H), "#.000000000000"); "  (fuori dalla carica: 0)"
  PRINT " curl E = "; VECCALC.CURL(Elec, 1, 1, 1, H, o)
  PRINT " ("; FORMAT(o(0), "#.000000"); ","; FORMAT(o(1), "#.000000"); ","; FORMAT(o(2), "#.000000"); ")  (campo conservativo)"
  PRINT " solenoidale = "; VECCALC.ISSOLENOIDAL(Elec, 1, 1, 1, H, 0.000001); "  irrotazionale = "; VECCALC.ISIRROTATIONAL(Elec, 1, 1, 1, H, 0.000001)
  PRINT " laplaciano(1/r) = "; FORMAT(VECCALC.LAPLACIAN(Pot, 1, 1, 1, H), "#.000000000000"); "  (armonica: 0)"

  ' ══════════════════════════════════════════════════════════════════════════
  ' B) ROTAZIONE RIGIDA
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "B) rotazione rigida G = (-y, x, 0)"
  PRINT " div G = "; FORMAT(VECCALC.DIVERGENCE(Spin, 1, 2, 3, H), "#.000000")
  PRINT " curl G = "; VECCALC.CURL(Spin, 1, 2, 3, H, o)
  PRINT " ("; FORMAT(o(0), "#.000000"); ","; FORMAT(o(1), "#.000000"); ","; FORMAT(o(2), "#.000000"); ")  atteso (0,0,2)"
  PRINT " solenoidale = "; VECCALC.ISSOLENOIDAL(Spin, 1, 2, 3, H, 0.000001); "  irrotazionale = "; VECCALC.ISIRROTATIONAL(Spin, 1, 2, 3, H, 0.000001)

  ' ══════════════════════════════════════════════════════════════════════════
  ' C) ELICA: lunghezza e triedro di Frenet
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "C) elica (cos t, sin t, t/5) su un giro"
  PRINT " lunghezza = "; FORMAT(VECCALC.ARCLENGTH(Elix, 0, 2 * PIVAL, 0.000001), "#.000000")
  PRINT " atteso    = "; FORMAT(2 * PIVAL * MATH.SQR(1 + 0.04), "#.000000")
  PRINT " TNB = "; VECCALC.TNB(Elix, 0, H, o)
  PRINT " kappa = "; FORMAT(o(9), "#.000000"); "  atteso "; FORMAT(1 / 1.04, "#.000000")
  PRINT " tau   = "; FORMAT(o(10), "#.000000"); "  atteso "; FORMAT(0.2 / 1.04, "#.000000")
  PRINT " T = ("; FORMAT(o(0), "#.000000"); ","; FORMAT(o(1), "#.000000"); ","; FORMAT(o(2), "#.000000"); ")"
  PRINT " N = ("; FORMAT(o(3), "#.000000"); ","; FORMAT(o(4), "#.000000"); ","; FORMAT(o(5), "#.000000"); ")"
  PRINT " B = ("; FORMAT(o(6), "#.000000"); ","; FORMAT(o(7), "#.000000"); ","; FORMAT(o(8), "#.000000"); ")"

  ' ══════════════════════════════════════════════════════════════════════════
  ' D) LAVORO lungo il cerchio
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "D) lavoro di G lungo il cerchio unitario = "; FORMAT(VECCALC.LINEWORK(Spin, Ring, 0, 2 * PIVAL, 0.000001), "#.000000"); "  atteso 2*pi = 6.283185"

  ' ══════════════════════════════════════════════════════════════════════════
  ' E) INTEGRALE DI LINEA SCALARE del potenziale
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "E) integrale di 1/r lungo il cerchio = "; FORMAT(VECCALC.LINESCALAR(Pot, Ring, 0, 2 * PIVAL, 0.000001), "#.000000"); "  (1/r = 1 sul cerchio)"

  ' ══════════════════════════════════════════════════════════════════════════
  ' F) FLUSSO attraverso il paraboloide
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "F) flusso di (0,0,1) attraverso z = x^2+y^2 sul quadrato unitario = "; FORMAT(VECCALC.FLUX(Up, Bowl, 0, 1, 0, 1, 0.000001), "#.000000")

  ' ══════════════════════════════════════════════════════════════════════════
  ' G) guardie
  ' ══════════════════════════════════════════════════════════════════════════
  DIM corto(1) AS DOUBLE
  PRINT "G) guardie: gradiente con out corto = "; VECCALC.GRADIENT(Pot, 1, 1, 1, H, corto)
  PRINT "            TNB con out corto = "; VECCALC.TNB(Elix, 0, H, corto)
  PRINT "fine"
END SUB
