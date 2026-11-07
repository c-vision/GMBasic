' FILE: Quad/QuadLab.bas
' SCENARIO DIDATTICO — misure di impianto: valore efficace di una tensione
' distorta, lavoro di una forza variabile, tempo di svuotamento di un serbatoio
' (integrale con singolarità all'estremo) e due quadrature costruite a mano sui
' nodi di Gauss.
'
' I valori esatti sono calcolabili a mano:
'   v(t) = 100 sin(wt) + 30 sin(3wt), un periodo T = 20 ms
'     RMS = sqrt(100^2/2 + 30^2/2) = sqrt(5450) = 73.824115
'   F(x) = 200 + 50 sin(x) da 0 a 2pi
'     lavoro = 200*2pi + 50*(1 - cos 2pi) = 400pi = 1256.637061
'   svuotamento: integrale di 1/sqrt(h) da 0 a 1 = 2  (singolare in h=0)
'   nodi di Gauss-Legendre a 4 nodi su [-1,1]: x^2 -> 2/3
'   nodi di Gauss-Hermite a 3 nodi: integrale di x^2 e^{-x^2} = sqrt(pi)/2
'
' Domini: QUAD. Output deterministico (la callback viene chiamata in modo sincrono).
IMPORT QUAD

CONST PIVAL = 3.14159265358979323846
CONST W2 = 314.1592653589793      ' 2*pi*50

' Tensione distorta: fondamentale a 50 Hz + terza armonica.
FUNCTION Tens(t AS DOUBLE) AS DOUBLE
  RETURN 100 * MATH.SIN(W2 * t) + 30 * MATH.SIN(3 * W2 * t)
END FUNCTION

' Forza che dipende dalla posizione.
FUNCTION Forza(x AS DOUBLE) AS DOUBLE
  RETURN 200 + 50 * MATH.SIN(x)
END FUNCTION

' Portata uscente da un serbatoio (Torricelli): 1/sqrt(h), singolare in h = 0.
FUNCTION Portata(h AS DOUBLE) AS DOUBLE
  IF h <= 0 THEN RETURN 0
  RETURN 1 / MATH.SQR(h)
END FUNCTION

' Integrande per le quadrature costruite sui nodi.
FUNCTION Quadro(x AS DOUBLE) AS DOUBLE
  RETURN x * x
END FUNCTION

SUB Main()
  DIM o(1) AS DOUBLE
  DIM xn(3) AS DOUBLE
  DIM wn(3) AS DOUBLE
  DIM xh(2) AS DOUBLE
  DIM wh(2) AS DOUBLE
  DIM i AS LONG
  DIM acc AS DOUBLE, rms AS DOUBLE, err AS DOUBLE

  ' ── 1. valore efficace di una tensione distorta (un periodo) ──────────────
  ' l'integrale è di v^2: la callback deve restituire il quadrato, quindi la
  ' calcolo qui dentro il MAIN per non duplicare funzioni.
  DIM T AS DOUBLE
  DIM ns AS LONG, k AS LONG
  DIM a0 AS DOUBLE, b0 AS DOUBLE, tot AS DOUBLE, toterr AS DOUBLE
  T = 0.02
  ' La regola G7-K15 da sola non risolve la terza armonica su un periodo: la stima
  ' d'errore lo dice, e allora si suddivide l'intervallo (quadratura composta).
  ns = 20
  tot = 0 : toterr = 0
  FOR k = 0 TO ns - 1
    a0 = k * T / ns
    b0 = (k + 1) * T / ns
    QUAD.KRONROD(Tens2, a0, b0, o)
    tot = tot + o(0)
    toterr = toterr + o(1)
  NEXT k
  PRINT "RMS: integrale v^2 su un periodo, suddiviso in "; ns; " intervalli"
  PRINT "  media = "; FORMAT(tot / T, "#.000000"); "  RMS = "; FORMAT(MATH.SQR(tot / T), "#.000000")
  PRINT "  errore stimato = "; FORMAT(toterr / T, "#.000000000000")
  PRINT "  atteso = 73.824115"
  rms = MATH.SQR(5450)
  err = MATH.ABS(MATH.SQR(tot / T) - rms)
  PRINT "  scarto dal valore esatto = "; FORMAT(err, "#.000000000000")

  ' ── 2. lavoro di una forza variabile ──────────────────────────────────────
  PRINT "Lavoro: F(x) = 200 + 50 sin(x) da 0 a 2pi"
  PRINT " gauss10 = "; FORMAT(QUAD.GAUSS(Forza, 0, 2 * PIVAL, 10), "#.000000")
  PRINT " gauss4  = "; FORMAT(QUAD.GAUSS(Forza, 0, 2 * PIVAL, 4), "#.000000")
  PRINT " romberg = "; FORMAT(QUAD.ROMBERG(Forza, 0, 2 * PIVAL, 0.0000000001, 15), "#.000000")
  PRINT " atteso  = "; FORMAT(400 * PIVAL, "#.000000")

  ' ── 3. tempo di svuotamento: integrale con singolarità in 0 ───────────────
  PRINT "Svuotamento: integrale di 1/sqrt(h) da 0 a 1"
  PRINT " tanh-sinh = "; FORMAT(QUAD.TANHSINH(Portata, 0, 1, 0.0000000001, 2000), "#.000000000000")
  PRINT " atteso    = 2"

  ' ── 4. quadratura a mano sui nodi di Gauss-Legendre (4 nodi) ──────────────
  PRINT "Gauss-Legendre a 4 nodi (x^2 su [-1,1] = 2/3)"
  PRINT " nodi = "; QUAD.LEGENDRENODES(4, xn, wn)
  acc = 0
  FOR i = 0 TO 3
    acc = acc + wn(i) * Quadro(xn(i))
  NEXT i
  PRINT " somma w(i)*f(x(i)) = "; FORMAT(acc, "#.000000000000")
  PRINT " nodo 0 = "; FORMAT(xn(0), "#.000000"); "  peso 0 = "; FORMAT(wn(0), "#.000000")

  ' ── 5. quadratura a mano sui nodi di Gauss-Hermite (3 nodi) ───────────────
  ' integrale di x^2 e^{-x^2} su R = sqrt(pi)/2 = 0.886227
  PRINT "Gauss-Hermite a 3 nodi (x^2 e^{-x^2} su R = sqrt(pi)/2)"
  PRINT " nodi = "; QUAD.HERMITENODES(3, xh, wh)
  acc = 0
  FOR i = 0 TO 2
    acc = acc + wh(i) * xh(i) * xh(i)
  NEXT i
  PRINT " somma = "; FORMAT(acc, "#.000000000000")
  PRINT " atteso = "; FORMAT(MATH.SQR(PIVAL) / 2, "#.000000000000")

  ' ── 6. guardie ────────────────────────────────────────────────────────────
  DIM corto(0) AS DOUBLE
  PRINT "guardia (out troppo corto) = "; QUAD.KRONROD(Tens2, 0, T, corto)
  PRINT "fine"
END SUB

' Quadrato della tensione: la quadratura integra v^2 per il valore efficace.
FUNCTION Tens2(t AS DOUBLE) AS DOUBLE
  DIM v AS DOUBLE
  v = Tens(t)
  RETURN v * v
END FUNCTION
