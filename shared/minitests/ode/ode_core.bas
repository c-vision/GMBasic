' ODE (FFI) — metodi avanzati: 8 funzioni con callback BASIC.
' Sistemi di prova con soluzione esatta:
'   y' = -y, y(0)=1                 -> y(1) = e^-1 = 0.367879441171442
'   oscillatore q'' = -q            -> un giro (t=2pi) riporta a (q,v) = (1,0)
'   y'' = -1, y(0)=y(1)=0           -> y = x(1-x)/2, quindi y(0.5) = 0.125
' EXPECT: ab4= 0|:0.367879442436||am4= 0|:0.367879441072||bdf2= 0|:0.367867730442||mid= 0|:0.367876375475||forced= 0|:0.841470984808 sin1=0.841470984808|verlet= 0|:1.000000,-0.000000|energia=1.000000||bvpfd= 0|:0.000000,0.125000,0.000000||bvpshoot= 0|:0.000000,0.125000,-0.000000||guard=-1|:-1||f=0
IMPORT ODE

CONST PIVAL = 3.14159265358979323846

' Decadimento: derivata = -y (sistema 1x1).
FUNCTION Decay(y() AS DOUBLE, t AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(0) AS DOUBLE
  r(0) = -y(0)
  RETURN r
END FUNCTION

' Oscillatore: stato (q, p), derivate (p, -q).
FUNCTION Osc(y() AS DOUBLE, t AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(1) AS DOUBLE
  r(0) = y(1)
  r(1) = -y(0)
  RETURN r
END FUNCTION

' Accelerazione per Verlet: riceve le POSIZIONI, restituisce a = -q.
FUNCTION Acc(y() AS DOUBLE, t AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(0) AS DOUBLE
  r(0) = -y(0)
  RETURN r
END FUNCTION

' Sistema NON autonomo: y' = cos(t), y(0)=0 -> y(1) = sin(1). Usa il tempo
' nella callback (forma array + scalare, descrittore 148).
FUNCTION Forced(y() AS DOUBLE, t AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(0) AS DOUBLE
  r(0) = MATH.COS(t)
  RETURN r
END FUNCTION

' BVP: derivata seconda = -1.
FUNCTION Bvp(x AS DOUBLE, y AS DOUBLE, dy AS DOUBLE) AS DOUBLE
  RETURN -1
END FUNCTION

SUB Main()
  DIM y0(0) AS DOUBLE
  DIM yout(0) AS DOUBLE
  DIM s2(1) AS DOUBLE
  DIM o2(1) AS DOUBLE

  ' ── y' = -y su [0,1] con 100 passi ────────────────────────────────────────
  y0(0) = 1
  PRINT "ab4="; ODE.ADAMSBASHFORTH4(Decay, y0, 0, 1, 100, yout)
  PRINT ":"; FORMAT(yout(0), "#.000000000000")
  PRINT "|am4="; ODE.ADAMSMOULTON4(Decay, y0, 0, 1, 100, yout)
  PRINT ":"; FORMAT(yout(0), "#.000000000000")
  PRINT "|bdf2="; ODE.BDF2(Decay, y0, 0, 1, 100, 20, 0.0000000001, yout)
  PRINT ":"; FORMAT(yout(0), "#.000000000000")
  PRINT "|mid="; ODE.IMPLICITMIDPOINT(Decay, y0, 0, 1, 100, 20, 0.0000000001, yout)
  PRINT ":"; FORMAT(yout(0), "#.000000000000")
  ' non autonomo: il tempo e' un parametro della callback
  y0(0) = 0
  PRINT "|forced="; ODE.ADAMSBASHFORTH4(Forced, y0, 0, 1, 1000, yout)
  PRINT ":"; FORMAT(yout(0), "#.000000000000"); " sin1="; FORMAT(MATH.SIN(1), "#.000000000000")

  ' ── oscillatore: un giro esatto con metodi simplettici ────────────────────
  s2(0) = 1 : s2(1) = 0
  s2(0) = 1 : s2(1) = 0
  PRINT "verlet="; ODE.VERLET(Acc, s2, 0, 2 * PIVAL, 10000, o2)
  PRINT ":"; FORMAT(o2(0), "#.000000"); ","; FORMAT(o2(1), "#.000000")
  PRINT " energia="; FORMAT(o2(0) * o2(0) + o2(1) * o2(1), "#.000000")

  ' ── problema ai limiti: y'' = -1, y(0)=y(1)=0, 11 nodi ───────────────────
  DIM g(10) AS DOUBLE
  PRINT "|bvpfd="; ODE.BVPFD(Bvp, 0, 1, 0, 0, g)
  PRINT ":"; FORMAT(g(0), "#.000000"); ","; FORMAT(g(5), "#.000000"); ","; FORMAT(g(10), "#.000000")
  DIM g2(10) AS DOUBLE
  PRINT "|bvpshoot="; ODE.BVPSHOOTING(Bvp, 0, 1, 0, 0, g2)
  PRINT ":"; FORMAT(g2(0), "#.000000"); ","; FORMAT(g2(5), "#.000000"); ","; FORMAT(g2(10), "#.000000")

  ' ── guardie: yout troppo corto, BVP con meno di 2 nodi ────────────────────
  DIM corto(0) AS DOUBLE
  s2(0) = 1 : s2(1) = 0
  PRINT "|guard="; ODE.VERLET(Acc, s2, 0, 1, 10, corto)
  PRINT ":"; ODE.BVPFD(Bvp, 0, 1, 0, 0, corto)
  PRINT "|f=0"
END SUB
