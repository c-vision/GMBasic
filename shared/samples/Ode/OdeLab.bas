' FILE: Ode/OdeLab.bas
' SCENARIO DIDATTICO — quattro problemi di evoluzione, ognuno col metodo adatto:
'
'   A) raffreddamento di Newton  T' = -k(T - Ta)  ->  T(60) = 20 + 80 e^-6
'      risolto con Adams-Bashforth 4, Adams-Moulton 4 e BDF2 (implicito)
'   B) sistema NON autonomo y' = cos(t): il tempo e' un parametro della callback,
'      quindi vale y(1) = sin(1)
'   C) oscillatore armonico: Verlet, con l'energia come prova
'   D) trave appoggiata con carico uniforme y'' = -1: differenze finite e shooting
'
' Nota sull'ABI: la callback del sistema riceve (y(), t) — array prima, tempo dopo,
' la forma che l'ABI regge su tutte e quattro le gambe. La callback del BVP è scalare
' e riceve x.
'
' Domini: ODE. Output deterministico.
IMPORT ODE

CONST PIVAL = 3.14159265358979323846

' A) Raffreddamento: T' = -k (T - Ta), k = 0.1, Ta = 20.
FUNCTION Freddo(y() AS DOUBLE, t AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(0) AS DOUBLE
  r(0) = -0.1 * (y(0) - 20)
  RETURN r
END FUNCTION

' B) Non autonomo: y' = cos(t). Il tempo arriva come secondo parametro della callback.
FUNCTION Forzato(y() AS DOUBLE, t AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(0) AS DOUBLE
  r(0) = MATH.COS(t)
  RETURN r
END FUNCTION

' C) Oscillatore: stato (q, p), derivate (p, -q).
FUNCTION Osc(y() AS DOUBLE, t AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(1) AS DOUBLE
  r(0) = y(1)
  r(1) = -y(0)
  RETURN r
END FUNCTION

' C) Accelerazione per Verlet (riceve le posizioni).
FUNCTION Acc(y() AS DOUBLE, t AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(0) AS DOUBLE
  r(0) = -y(0)
  RETURN r
END FUNCTION

' D) Trave: derivata seconda = -1.
FUNCTION Trave(x AS DOUBLE, y AS DOUBLE, dy AS DOUBLE) AS DOUBLE
  RETURN -1
END FUNCTION

SUB Main()
  DIM y0(0) AS DOUBLE
  DIM yout(0) AS DOUBLE
  DIM st(1) AS DOUBLE
  DIM out2(1) AS DOUBLE
  DIM g(10) AS DOUBLE
  DIM g2(10) AS DOUBLE
  DIM exact AS DOUBLE, err AS DOUBLE

  ' ══════════════════════════════════════════════════════════════════════════
  ' A) RAFFREDDAMENTO DI NEWTON — T(60) con tre metodi
  ' ══════════════════════════════════════════════════════════════════════════
  y0(0) = 100
  exact = 20 + 80 * MATH.EXP(-0.1 * 60)
  PRINT "A) raffreddamento: T(0)=100, Ta=20, k=0.1, t=60 s"
  PRINT " esatto = "; FORMAT(exact, "#.000000")
  PRINT " ab4    = "; ODE.ADAMSBASHFORTH4(Freddo, y0, 0, 60, 600, yout)
  PRINT "  "; FORMAT(yout(0), "#.000000")
  PRINT " am4    = "; ODE.ADAMSMOULTON4(Freddo, y0, 0, 60, 600, yout)
  PRINT "  "; FORMAT(yout(0), "#.000000")
  PRINT " bdf2   = "; ODE.BDF2(Freddo, y0, 0, 60, 600, 30, 0.0000000001, yout)
  PRINT "  "; FORMAT(yout(0), "#.000000")
  PRINT " mid    = "; ODE.IMPLICITMIDPOINT(Freddo, y0, 0, 60, 600, 30, 0.0000000001, yout)
  PRINT "  "; FORMAT(yout(0), "#.000000")

  ' ══════════════════════════════════════════════════════════════════════════
  ' B) NON AUTONOMO: y' = cos(t), il tempo è un parametro della callback
  ' ══════════════════════════════════════════════════════════════════════════
  st(0) = 0
  PRINT "B) y' = cos(t), y(0)=0, fino a t=1: il tempo è un parametro"
  PRINT " ab4 = "; ODE.ADAMSBASHFORTH4(Forzato, st, 0, 1, 1000, out2)
  PRINT "  y = "; FORMAT(out2(0), "#.000000000000"); "  atteso sin(1) = "; FORMAT(MATH.SIN(1), "#.000000000000")
  err = MATH.ABS(out2(0) - MATH.SIN(1))
  PRINT "  scarto = "; FORMAT(err, "#.000000000000")

  ' ══════════════════════════════════════════════════════════════════════════
  ' C) OSCILLATORE: energia dopo un giro (t = 2pi)
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "C) oscillatore q'' = -q, un giro (t = 2pi), 1000 passi"
  st(0) = 1 : st(1) = 0
  PRINT " verlet = "; ODE.VERLET(Acc, st, 0, 2 * PIVAL, 1000, out2)
  PRINT "  q = "; FORMAT(out2(0), "#.000000"); "  energia = "; FORMAT(out2(0) * out2(0) + out2(1) * out2(1), "#.000000")

  ' ══════════════════════════════════════════════════════════════════════════
  ' D) TRAVE APPOGGIATA: derivata seconda = -1, y(0)=y(1)=0 -> y = x(1-x)/2
  ' ══════════════════════════════════════════════════════════════════════════
  ' ai nodi x = 0.2, 0.5, 0.8: y = 0.08, 0.125, 0.08
  PRINT "D) trave con carico uniforme, 11 nodi"
  PRINT " bvpfd = "; ODE.BVPFD(Trave, 0, 1, 0, 0, g)
  PRINT "  y(0.2) = "; FORMAT(g(2), "#.00000000"); "  y(0.5) = "; FORMAT(g(5), "#.00000000"); "  y(0.8) = "; FORMAT(g(8), "#.00000000")
  PRINT "  esatto = "; FORMAT(TraveEsatta(0.2), "#.00000000"); "  "; FORMAT(TraveEsatta(0.5), "#.00000000"); "  "; FORMAT(TraveEsatta(0.8), "#.00000000")
  PRINT " bvpshoot = "; ODE.BVPSHOOTING(Trave, 0, 1, 0, 0, g2)
  PRINT "  y(0.5) = "; FORMAT(g2(5), "#.00000000")

  ' ══════════════════════════════════════════════════════════════════════════
  ' E) guardie
  ' ══════════════════════════════════════════════════════════════════════════
  DIM corto(0) AS DOUBLE
  st(0) = 1 : st(1) = 0
  PRINT "guardie: yout troppo corto = "; ODE.VERLET(Acc, st, 0, 1, 10, corto)
  PRINT "         BVP con un solo nodo = "; ODE.BVPFD(Trave, 0, 1, 0, 0, corto)
  PRINT "fine"
END SUB

' Soluzione esatta con carico uniforme: y = x(1 - x)/2.
FUNCTION TraveEsatta(x AS DOUBLE) AS DOUBLE
  RETURN x * (1 - x) / 2
END FUNCTION
