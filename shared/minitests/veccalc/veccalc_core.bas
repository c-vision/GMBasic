' VECCALC (FFI) — 11 funzioni di calcolo vettoriale su campi con derivate esatte.
' Campi di prova:
'   f(x,y,z) = x^2 + y^2 + z^2   -> grad = (2x,2y,2z), laplaciano = 6
'   F(x,y,z) = (x, y, z)         -> div = 3, curl = 0
'   G(x,y,z) = (-y, x, 0)        -> div = 0, curl = (0,0,2)
'   cerchio r(t) = (cos t, sin t, 0)   -> lunghezza 2*pi, kappa 1, tau 0
' EXPECT: grad= 0|:2.000000,4.000000,6.000000||lap=6.000000||div-rad=3.000000||curl-rad= 0|:0.000000,0.000000,0.000000||div-swirl=0.000000||curl-swirl= 0|:0.000000,0.000000,2.000000||solen= 1||irrot= 1||no= 0 , 0||len=6.283185||tnb= 0|:0.000000,1.000000,0.000000|N=-1.000000,0.000000,0.000000|B=0.000000,0.000000,1.000000|k=1.000000 t=0.000000||ls=6.283185||lw=6.283185||flux=1.000000||guard=-1|:-1|:-1||f=0
IMPORT VECCALC

CONST PIVAL = 3.14159265358979323846

' Campo scalare: distanza al quadrato dall'origine.
FUNCTION Sph(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS DOUBLE
  RETURN x * x + y * y + z * z
END FUNCTION

' Campo vettoriale radiale F = (x, y, z).
FUNCTION Rad(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(2) AS DOUBLE
  r(0) = x
  r(1) = y
  r(2) = z
  RETURN r
END FUNCTION

' Campo di rotazione rigida G = (-y, x, 0): solenoidale, non irrotazionale.
FUNCTION Swirl(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(2) AS DOUBLE
  r(0) = -y
  r(1) = x
  r(2) = 0
  RETURN r
END FUNCTION

' Cerchio unitario nel piano z = 0.
FUNCTION Circle(t AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(2) AS DOUBLE
  r(0) = MATH.COS(t)
  r(1) = MATH.SIN(t)
  r(2) = 0
  RETURN r
END FUNCTION

' Quadrato unitario nel piano z = 0 (S(u,v) = (u, v, 0)); Su x Sv = +z.
FUNCTION Plane(u AS DOUBLE, v AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(2) AS DOUBLE
  r(0) = u
  r(1) = v
  r(2) = 0
  RETURN r
END FUNCTION

' Campo unitario lungo z, per il flusso.
FUNCTION Up(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(2) AS DOUBLE
  r(0) = 0
  r(1) = 0
  r(2) = 1
  RETURN r
END FUNCTION

SUB Main()
  DIM o(10) AS DOUBLE
  DIM h AS DOUBLE
  h = 0.0001

  ' ── campo scalare: gradiente e laplaciano ─────────────────────────────────
  PRINT "grad="; VECCALC.GRADIENT(Sph, 1, 2, 3, h, o)
  PRINT ":"; FORMAT(o(0), "#.000000"); ","; FORMAT(o(1), "#.000000"); ","; FORMAT(o(2), "#.000000")
  PRINT "|lap="; FORMAT(VECCALC.LAPLACIAN(Sph, 1, 2, 3, h), "#.000000")

  ' ── campo vettoriale: divergenza e rotore ─────────────────────────────────
  PRINT "|div-rad="; FORMAT(VECCALC.DIVERGENCE(Rad, 1, 2, 3, h), "#.000000")
  PRINT "|curl-rad="; VECCALC.CURL(Rad, 1, 2, 3, h, o)
  PRINT ":"; FORMAT(o(0), "#.000000"); ","; FORMAT(o(1), "#.000000"); ","; FORMAT(o(2), "#.000000")
  PRINT "|div-swirl="; FORMAT(VECCALC.DIVERGENCE(Swirl, 1, 2, 3, h), "#.000000")
  PRINT "|curl-swirl="; VECCALC.CURL(Swirl, 1, 2, 3, h, o)
  PRINT ":"; FORMAT(o(0), "#.000000"); ","; FORMAT(o(1), "#.000000"); ","; FORMAT(o(2), "#.000000")
  PRINT "|solen="; VECCALC.ISSOLENOIDAL(Swirl, 1, 2, 3, h, 0.0001)
  PRINT "|irrot="; VECCALC.ISIRROTATIONAL(Rad, 1, 2, 3, h, 0.0001)
  PRINT "|no="; VECCALC.ISSOLENOIDAL(Rad, 1, 2, 3, h, 0.0001); ","; VECCALC.ISIRROTATIONAL(Swirl, 1, 2, 3, h, 0.0001)

  ' ── curva: lunghezza d'arco e triedro di Frenet ───────────────────────────
  PRINT "|len="; FORMAT(VECCALC.ARCLENGTH(Circle, 0, 2 * PIVAL, 0.000001), "#.000000")
  PRINT "|tnb="; VECCALC.TNB(Circle, 0, 0.0001, o)
  PRINT ":"; FORMAT(o(0), "#.000000"); ","; FORMAT(o(1), "#.000000"); ","; FORMAT(o(2), "#.000000")
  PRINT " N="; FORMAT(o(3), "#.000000"); ","; FORMAT(o(4), "#.000000"); ","; FORMAT(o(5), "#.000000")
  PRINT " B="; FORMAT(o(6), "#.000000"); ","; FORMAT(o(7), "#.000000"); ","; FORMAT(o(8), "#.000000")
  PRINT " k="; FORMAT(o(9), "#.000000"); " t="; FORMAT(o(10), "#.000000")

  ' ── integrali di linea e flusso ───────────────────────────────────────────
  ' f = x^2+y^2 vale 1 sul cerchio: integrale di f ds = 2*pi
  PRINT "|ls="; FORMAT(VECCALC.LINESCALAR(Sph, Circle, 0, 2 * PIVAL, 0.000001), "#.000000")
  ' G . r' = 1 sul cerchio: lavoro = 2*pi
  PRINT "|lw="; FORMAT(VECCALC.LINEWORK(Swirl, Circle, 0, 2 * PIVAL, 0.000001), "#.000000")
  ' campo (0,0,1) attraverso il quadrato unitario in z=0: flusso = 1
  PRINT "|flux="; FORMAT(VECCALC.FLUX(Up, Plane, 0, 1, 0, 1, 0.000001), "#.000000")

  ' ── guardie ───────────────────────────────────────────────────────────────
  DIM corto(1) AS DOUBLE
  PRINT "|guard="; VECCALC.GRADIENT(Sph, 1, 2, 3, h, corto)
  PRINT ":"; VECCALC.CURL(Rad, 1, 2, 3, h, corto)
  PRINT ":"; VECCALC.TNB(Circle, 0, h, corto)
  PRINT "|f=0"
END SUB
