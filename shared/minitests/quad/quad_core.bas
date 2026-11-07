' QUAD (FFI) — 7 funzioni di quadratura, con callback BASIC.
' Integrali scelti per avere un valore esatto a mano:
'   f1(x) = x^2            su [0,1] -> 1/3
'   f2(x) = 1/sqrt(x)      su [0,1] -> 2      (singolare all'estremo: tanh-sinh)
'   f3(x) = 4/(1+x^2)      su [0,1] -> pi     (Gauss-Legendre, approssimato)
' EXPECT: kronrod= 0|:0.333333333333,0.000000000000||gauss3=0.333333333333||gauss5=3.141592639885||romberg=0.333333333333||tanhsinh=1.999999989159||tanhcomp=0.333333333333||leg3= 3|:-0.774597,0.555556,0.000000,0.888889||her3= 3|:1.224745,0.295409,-1.224745,0.295409||guard=-1|:-1||f=0
IMPORT QUAD

CONST PIVAL = 3.14159265358979323846

' Polinomio: i metodi esatti lo integrano senza errore.
FUNCTION Sq(x AS DOUBLE) AS DOUBLE
  RETURN x * x
END FUNCTION

' Integranda con singolarità all'estremo sinistro. BASIC segnala la divisione per
' zero (il C no): la callback deve gestire l'estremo, che la quadratura pesa ~0.
FUNCTION Sing(x AS DOUBLE) AS DOUBLE
  IF x <= 0 THEN RETURN 0
  RETURN 1 / MATH.SQR(x)
END FUNCTION

' Razionale: il valore esatto è pi greco.
FUNCTION Rat(x AS DOUBLE) AS DOUBLE
  RETURN 4 / (1 + x * x)
END FUNCTION

' Forma a due argomenti (x e distanza dall'estremo): qui è un polinomio.
FUNCTION Comp(x AS DOUBLE, d AS DOUBLE) AS DOUBLE
  RETURN x * x
END FUNCTION

SUB Main()
  DIM o(1) AS DOUBLE
  DIM x(2) AS DOUBLE
  DIM w(2) AS DOUBLE
  DIM xh(2) AS DOUBLE
  DIM wh(2) AS DOUBLE

  ' ── Gauss-Kronrod su x^2: 1/3, errore stimato piccolissimo ────────────────
  PRINT "kronrod="; QUAD.KRONROD(Sq, 0, 1, o)
  PRINT ":"; FORMAT(o(0), "#.000000000000"); ","; FORMAT(o(1), "#.000000000000")
  ' ── Gauss-Legendre: 3 nodi esatti per x^2; 5 nodi per il razionale ────────
  PRINT "|gauss3="; FORMAT(QUAD.GAUSS(Sq, 0, 1, 3), "#.000000000000")
  PRINT "|gauss5="; FORMAT(QUAD.GAUSS(Rat, 0, 1, 5), "#.000000000000")
  ' ── Romberg ───────────────────────────────────────────────────────────────
  PRINT "|romberg="; FORMAT(QUAD.ROMBERG(Sq, 0, 1, 0.0000000001, 12), "#.000000000000")
  ' ── tanh-sinh: gestisce la singolarità in 0 (integrale = 2) ───────────────
  PRINT "|tanhsinh="; FORMAT(QUAD.TANHSINH(Sing, 0, 1, 0.0000000001, 5000), "#.000000000000")
  ' ── variante a due argomenti ──────────────────────────────────────────────
  PRINT "|tanhcomp="; FORMAT(QUAD.TANHSINHCOMP(Comp, 0, 1, 0.0000000001, 5000), "#.000000000000")
  ' ── nodi e pesi di Gauss-Legendre a 3 nodi ────────────────────────────────
  ' x = (-sqrt(3/5), 0, sqrt(3/5)) = (-0.774597, 0, 0.774597)
  ' w = (5/9, 8/9, 5/9) = (0.555556, 0.888889, 0.555556)
  PRINT "|leg3="; QUAD.LEGENDRENODES(3, x, w)
  PRINT ":"; FORMAT(x(0), "#.000000"); ","; FORMAT(w(0), "#.000000"); ","; FORMAT(x(1), "#.000000"); ","; FORMAT(w(1), "#.000000")
  ' ── nodi e pesi di Gauss-Hermite a 3 nodi ─────────────────────────────────
  ' x = (-sqrt(3/2), 0, sqrt(3/2)) = (-1.224745, 0, 1.224745)
  ' w = (sqrt(pi)/6, 2*sqrt(pi)/3, sqrt(pi)/6) = (0.295409, 1.181636, 0.295409)
  PRINT "|her3="; QUAD.HERMITENODES(3, xh, wh)
  PRINT ":"; FORMAT(xh(0), "#.000000"); ","; FORMAT(wh(0), "#.000000"); ","; FORMAT(xh(2), "#.000000"); ","; FORMAT(wh(2), "#.000000")
  ' ── guardie: array troppo corto ───────────────────────────────────────────
  DIM c1(0) AS DOUBLE
  PRINT "|guard="; QUAD.KRONROD(Sq, 0, 1, c1)
  PRINT ":"; QUAD.LEGENDRENODES(3, c1, c1)
  PRINT "|f=0"
END SUB
