' FILE: neural_dense_copy_merge.bas
' NEURAL — copia di pesi e fusione pesata (model averaging) con la superficie esistente.
'
' gmbneural ha gn_net_copy / gn_net_merge sulla struct `gn_net`; qui la stessa
' capacita' si compone da DENSEPARAMS + DENSESETPARAMS, che sono gia' il canale
' di lettura/scrittura dei pesi di uno strato denso. Nessuna API duplicata:
'   copia  : C <- A                     (params di A -> params di C)
'   fusione: C <- (1-w)*A + w*B         (media pesata di W e b)
' Poiche' lo strato denso e' lineare (y = W x + b), l'uscita di C deve coincidere
' con la combinazione delle uscite di A e B: e' l'asserzione del test.
' EXPECT: copy=3.070104,3.070104|:0.000000,0.000000||merge=1.674977,1.026460|:1.674977,1.026460|:0.000000|:0.000000||free= 0  0  0||f=0
IMPORT NEURAL

SUB Main()
  DIM hA AS LONG, hB AS LONG, hC AS LONG
  hA = NEURAL.DENSEINIT(3, 2, 1)
  hB = NEURAL.DENSEINIT(3, 2, 7)
  hC = NEURAL.DENSEINIT(3, 2, 99)

  DIM x(2) AS DOUBLE
  x(0)=0.5 : x(1)=-0.25 : x(2)=0.75
  DIM ya(1) AS DOUBLE, yb(1) AS DOUBLE, ym(1) AS DOUBLE
  w = NEURAL.DENSEFORWARD(hA, x, 1, ya)
  w = NEURAL.DENSEFORWARD(hB, x, 1, yb)

  ' W e' out*in = 2*3 = 6 valori; b ha out = 2 valori
  DIM Wa(5) AS DOUBLE, ba(1) AS DOUBLE
  DIM Wb(5) AS DOUBLE, bb(1) AS DOUBLE
  DIM Wm(5) AS DOUBLE, bm(1) AS DOUBLE
  w = NEURAL.DENSEPARAMS(hA, Wa, ba)
  w = NEURAL.DENSEPARAMS(hB, Wb, bb)

  ' ── copia: C <- A ────────────────────────────────────────────────────────
  w = NEURAL.DENSESETPARAMS(hC, Wa, ba)
  w = NEURAL.DENSEFORWARD(hC, x, 1, ym)
  PRINT "copy="; FORMAT(ym(0), "#.000000"); ","; FORMAT(ya(0), "#.000000")
  PRINT ":"; FORMAT(ym(0) - ya(0), "#.000000"); ","; FORMAT(ym(1) - ya(1), "#.000000")

  ' ── fusione: C <- 0.75*A + 0.25*B ───────────────────────────────────────
  DIM i AS LONG
  FOR i = 0 TO 5
    Wm(i) = 0.75 * Wa(i) + 0.25 * Wb(i)
  NEXT i
  FOR i = 0 TO 1
    bm(i) = 0.75 * ba(i) + 0.25 * bb(i)
  NEXT i
  w = NEURAL.DENSESETPARAMS(hC, Wm, bm)
  w = NEURAL.DENSEFORWARD(hC, x, 1, ym)
  PRINT "|merge="; FORMAT(ym(0), "#.000000"); ","; FORMAT(ym(1), "#.000000")
  PRINT ":"; FORMAT(0.75 * ya(0) + 0.25 * yb(0), "#.000000"); ","; FORMAT(0.75 * ya(1) + 0.25 * yb(1), "#.000000")
  PRINT ":"; FORMAT(ABS(ym(0) - (0.75 * ya(0) + 0.25 * yb(0))), "#.000000")
  PRINT ":"; FORMAT(ABS(ym(1) - (0.75 * ya(1) + 0.25 * yb(1))), "#.000000")

  PRINT "|free="; NEURAL.DENSEFREE(hA); NEURAL.DENSEFREE(hB); NEURAL.DENSEFREE(hC)
  PRINT "|f=0"
END SUB
