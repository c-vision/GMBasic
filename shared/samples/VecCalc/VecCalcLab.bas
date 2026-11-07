' FILE: VecCalc/VecCalcLab.bas
' SCENARIO DIDATTICO — calcolo vettoriale su campi con derivate note.
'
' VECCALC lavora sui campi come FORMULE, non su griglie: si passa una FUNCTION e il
' punto in cui valutarla, e le derivate sono differenze centrali di passo `h`. Ogni
' sezione qui sotto confronta il risultato numerico con il valore ANALITICO e ne
' stampa l'errore: e' il modo corretto di usare un metodo alle differenze finite.
'
'   A)  campo elettrostatico E(r) = r/|r|^3 e potenziale V = 1/|r|:
'       grad V = -E, div E = 0 (solenoidale), curl E = 0 (conservativo),
'       laplaciano(1/r) = 0 (armonica)              -> le identita' di Maxwell
'   A2) campo NON simmetrico: f = x*y*z e F = (x^2, y^2, z^2) -> le tre derivate
'       parziali devono essere DISTINTE (un'implementazione che confonde gli assi
'       passa i campi simmetrici ma sbaglia questi)
'   B)  rotazione rigida G = (-y, x, 0): div = 0 ma curl = (0,0,2)
'   C)  elica r(t) = (cos t, sin t, t/5): lunghezza 2*pi*sqrt(1+a^2),
'       curvatura kappa = 1/(1+a^2), torsione tau = a/(1+a^2) con a = 1/5
'   C2) retta r(t) = (t, 2t, 3t): kappa = tau = 0
'   D)  lavoro di G lungo il cerchio unitario: 2*pi
'   E)  integrale di linea scalare di V = 1/r lungo lo stesso cerchio: 2*pi
'   F)  flusso di (0,0,1) attraverso il paraboloide z = x^2+y^2: 1
'   G)  guardie: array di uscita corto (-1), passo h = 0 (-1 o 0)
'
' Parametri ricorrenti:
'   h   = passo delle differenze centrali (1e-4: errore O(h^2) ~ 1e-8)
'   tol = tolleranza di Simpson adattiva per gli integrali (1e-6)
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

' Campo scalare NON simmetrico: f = x*y*z -> grad = (y z, x z, x y).
FUNCTION Trip(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS DOUBLE
  RETURN x * y * z
END FUNCTION

' Campo vettoriale NON simmetrico: F = (x^2, y^2, z^2) -> div = 2(x+y+z).
FUNCTION Squares(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(2) AS DOUBLE
  r(0) = x * x
  r(1) = y * y
  r(2) = z * z
  RETURN r
END FUNCTION

' Retta r(t) = (t, 2t, 3t).
FUNCTION Straight(t AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(2) AS DOUBLE
  r(0) = t
  r(1) = 2 * t
  r(2) = 3 * t
  RETURN r
END FUNCTION

SUB Main()
  DIM o(10) AS DOUBLE          ' uscita di GRADIENT/CURL/TNB (11 slot per TNB)
  DIM i AS LONG
  DIM v AS DOUBLE

  ' ══════════════════════════════════════════════════════════════════════════
  ' A) CAMPO ELETTROSTATICO — le identita' di Maxwell
  '    V = 1/|r|, E = r/|r|^3, punto (1,1,1): |r| = sqrt(3), E = (1,1,1)/(3*sqrt(3))
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "A) carica puntiforme al centro, punto (1,1,1), |r| = "; FORMAT(MATH.SQR(3), "#.000000")
  v = 3 * MATH.SQR(3)          ' |r|^3 = 3*sqrt(3)
  PRINT " grad V (esito "; VECCALC.GRADIENT(Pot, 1, 1, 1, H, o); ") = (";
  PRINT FORMAT(o(0), "#.000000"); ","; FORMAT(o(1), "#.000000"); ","; FORMAT(o(2), "#.000000"); ")"
  PRINT " errore |grad V + E| = "; FORMAT(MATH.SQR((o(0) + 1 / v) ^ 2 + (o(1) + 1 / v) ^ 2 + (o(2) + 1 / v) ^ 2), "#.000000000000")
  PRINT " div E = "; FORMAT(VECCALC.DIVERGENCE(Elec, 1, 1, 1, H), "#.000000000000"); "  (fuori dalla carica: 0)"
  PRINT " curl E (esito "; VECCALC.CURL(Elec, 1, 1, 1, H, o); ") = (";
  PRINT FORMAT(o(0), "#.000000"); ","; FORMAT(o(1), "#.000000"); ","; FORMAT(o(2), "#.000000"); ")  (conservativo)"
  PRINT " solenoidale = "; VECCALC.ISSOLENOIDAL(Elec, 1, 1, 1, H, 0.000001);
  PRINT "  irrotazionale = "; VECCALC.ISIRROTATIONAL(Elec, 1, 1, 1, H, 0.000001)
  PRINT " laplaciano(1/r) = "; FORMAT(VECCALC.LAPLACIAN(Pot, 1, 1, 1, H), "#.000000000000"); "  (armonica: 0)"

  ' ══════════════════════════════════════════════════════════════════════════
  ' A2) DERIVATE NON SIMMETRICHE — f = x*y*z e F = (x^2, y^2, z^2) in (2,3,4)
  '     grad f = (y z, x z, x y) = (12, 8, 6); div F = 2(x+y+z) = 18
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "A2) derivate non simmetriche in (2,3,4)"
  PRINT " grad(x*y*z) (esito "; VECCALC.GRADIENT(Trip, 2, 3, 4, H, o); ") = (";
  PRINT FORMAT(o(0), "#.000000"); ","; FORMAT(o(1), "#.000000"); ","; FORMAT(o(2), "#.000000"); ")  atteso (12,8,6)"
  PRINT " div(x^2,y^2,z^2) = "; FORMAT(VECCALC.DIVERGENCE(Squares, 2, 3, 4, H), "#.000000"); "  atteso 18"

  ' ══════════════════════════════════════════════════════════════════════════
  ' B) ROTAZIONE RIGIDA — solenoidale ma NON irrotazionale
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "B) rotazione rigida G = (-y, x, 0)"
  PRINT " div G = "; FORMAT(VECCALC.DIVERGENCE(Spin, 1, 2, 3, H), "#.000000")
  PRINT " curl G (esito "; VECCALC.CURL(Spin, 1, 2, 3, H, o); ") = (";
  PRINT FORMAT(o(0), "#.000000"); ","; FORMAT(o(1), "#.000000"); ","; FORMAT(o(2), "#.000000"); ")  atteso (0,0,2)"
  PRINT " solenoidale = "; VECCALC.ISSOLENOIDAL(Spin, 1, 2, 3, H, 0.000001);
  PRINT "  irrotazionale = "; VECCALC.ISIRROTATIONAL(Spin, 1, 2, 3, H, 0.000001)

  ' ══════════════════════════════════════════════════════════════════════════
  ' C) ELICA — lunghezza d'arco e triedro di Frenet
  '    a = 1/5: L = 2*pi*sqrt(1+a^2), kappa = 1/(1+a^2), tau = a/(1+a^2)
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "C) elica (cos t, sin t, t/5) su un giro"
  v = VECCALC.ARCLENGTH(Elix, 0, 2 * PIVAL, 0.000001)
  PRINT " lunghezza = "; FORMAT(v, "#.000000"); "  atteso "; FORMAT(2 * PIVAL * MATH.SQR(1.04), "#.000000");
  PRINT "  errore = "; FORMAT(MATH.ABS(v - 2 * PIVAL * MATH.SQR(1.04)), "#.000000000000")
  PRINT " TNB (esito "; VECCALC.TNB(Elix, 0, H, o); ")"
  PRINT " T = ("; FORMAT(o(0), "#.000000"); ","; FORMAT(o(1), "#.000000"); ","; FORMAT(o(2), "#.000000"); ")"
  PRINT " N = ("; FORMAT(o(3), "#.000000"); ","; FORMAT(o(4), "#.000000"); ","; FORMAT(o(5), "#.000000"); ")"
  PRINT " B = ("; FORMAT(o(6), "#.000000"); ","; FORMAT(o(7), "#.000000"); ","; FORMAT(o(8), "#.000000"); ")"
  PRINT " kappa = "; FORMAT(o(9), "#.000000"); "  atteso "; FORMAT(1 / 1.04, "#.000000");
  PRINT "  errore = "; FORMAT(MATH.ABS(o(9) - 1 / 1.04), "#.000000000000")
  PRINT " tau   = "; FORMAT(o(10), "#.000000"); "  atteso "; FORMAT(0.2 / 1.04, "#.000000");
  PRINT "  errore = "; FORMAT(MATH.ABS(o(10) - 0.2 / 1.04), "#.000000000000")

  ' ══════════════════════════════════════════════════════════════════════════
  ' C2) RETTA — caso limite: curvatura e torsione nulle (mai rumore amplificato)
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "C2) retta (t, 2t, 3t)"
  PRINT " TNB (esito "; VECCALC.TNB(Straight, 1, H, o); ")  kappa = "; FORMAT(o(9), "#.000000");
  PRINT "  tau = "; FORMAT(o(10), "#.000000")

  ' ══════════════════════════════════════════════════════════════════════════
  ' D) LAVORO — integrale di linea di G lungo il cerchio unitario = 2*pi
  ' ══════════════════════════════════════════════════════════════════════════
  v = VECCALC.LINEWORK(Spin, Ring, 0, 2 * PIVAL, 0.000001)
  PRINT "D) lavoro di G lungo il cerchio = "; FORMAT(v, "#.000000");
  PRINT "  errore vs 2*pi = "; FORMAT(MATH.ABS(v - 2 * PIVAL), "#.000000000000")

  ' ══════════════════════════════════════════════════════════════════════════
  ' E) INTEGRALE DI LINEA SCALARE — V = 1/r vale 1 sul cerchio -> 2*pi
  ' ══════════════════════════════════════════════════════════════════════════
  v = VECCALC.LINESCALAR(Pot, Ring, 0, 2 * PIVAL, 0.000001)
  PRINT "E) integrale di 1/r lungo il cerchio = "; FORMAT(v, "#.000000");
  PRINT "  errore vs 2*pi = "; FORMAT(MATH.ABS(v - 2 * PIVAL), "#.000000000000")

  ' ══════════════════════════════════════════════════════════════════════════
  ' F) FLUSSO — campo (0,0,1) attraverso il paraboloide sul quadrato unitario = 1
  ' ══════════════════════════════════════════════════════════════════════════
  v = VECCALC.FLUX(Up, Bowl, 0, 1, 0, 1, 0.000001)
  PRINT "F) flusso attraverso z = x^2+y^2 = "; FORMAT(v, "#.000000");
  PRINT "  errore vs 1 = "; FORMAT(MATH.ABS(v - 1), "#.000000000000")

  ' ══════════════════════════════════════════════════════════════════════════
  ' G) GUARDIE — array di uscita corto e passo h = 0
  ' ══════════════════════════════════════════════════════════════════════════
  DIM corto(1) AS DOUBLE
  PRINT "G) out corto: gradiente = "; VECCALC.GRADIENT(Pot, 1, 1, 1, H, corto);
  PRINT "  curl = "; VECCALC.CURL(Spin, 1, 2, 3, H, corto);
  PRINT "  TNB = "; VECCALC.TNB(Elix, 0, H, corto)
  PRINT "   h=0: gradiente = "; VECCALC.GRADIENT(Pot, 1, 1, 1, 0, o);
  PRINT "  divergenza = "; FORMAT(VECCALC.DIVERGENCE(Spin, 1, 2, 3, 0), "#.000000");
  PRINT "  laplaciano = "; FORMAT(VECCALC.LAPLACIAN(Pot, 1, 1, 1, 0), "#.000000")
  PRINT "fine"
END SUB
