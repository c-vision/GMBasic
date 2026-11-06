' FILE: Neural/NeuralWorkbench.bas
' SCENARIO DIDATTICO — un piccolo studio di modellistica con il pacchetto NEURAL:
' regressione lineare addestrata, serie temporale, classificazione ad albero,
' compressione dei pesi, derivata simbolica e scoperta di una formula con la GP.
'
' Dati (tutti noti a mano, così i risultati si possono verificare):
'   regressione   y = 2*a + b  su   (a,b) = (1,2) (2,3) (3,4) (4,5) → 4, 7, 10, 13
'   serie         x = 0.1 0.2 0.3  (una feature, 3 passi)
'   classifica    x = 1..6  →  classe 0 per 1..3, classe 1 per 4..6
'   formula GP    bersaglio x + y*y = 11 con x = 2, y = 3
'
' Il programma mostra il ciclo completo di ogni famiglia: inizializzazione,
' forward, perdita/gradiente, backward, passo dell'ottimizzatore, uso del modello.
' Domini: NEURAL. Output deterministico (seed fissi).
IMPORT NEURAL

' Fitness per la GP: valuta il genoma come espressione in notazione prefissa.
' Codici: 1 = x (terminale), 2 = y (terminale), 3 = somma, 4 = prodotto.
' Il genoma è una sequenza di codici; lo si valuta con una pila esplicita.
FUNCTION FormulaFitness(code() AS DOUBLE) AS DOUBLE
  DIM st(63) AS DOUBLE, sp AS LONG, i AS LONG, a AS DOUBLE, b AS DOUBLE, c AS LONG
  sp = 0
  ' notazione prefissa: si valuta dall'ultimo codice al primo (pila)
  FOR i = UBOUND(code) TO 0 STEP -1
    c = code(i)
    IF c = 1 THEN
      sp = sp + 1 : st(sp) = 2
    ELSEIF c = 2 THEN
      sp = sp + 1 : st(sp) = 3
    ELSEIF c = 3 OR c = 4 THEN
      IF sp < 2 THEN RETURN -1000000
      b = st(sp) : sp = sp - 1
      a = st(sp) : sp = sp - 1
      IF c = 3 THEN
        sp = sp + 1 : st(sp) = a + b
      ELSE
        sp = sp + 1 : st(sp) = a * b
      END IF
    ELSE
      RETURN -1000000
    END IF
  NEXT i
  IF sp <> 1 THEN RETURN -1000000
  ' fitness da massimizzare: negativa dell'errore assoluto sul bersaglio 11
  RETURN -ABS(st(1) - 11)
END FUNCTION

SUB Main()
  ' ══ 1) regressione lineare addestrata (strato denso + MSE) ══════════════
  DIM X(7) AS DOUBLE, T(3) AS DOUBLE, Y(3) AS DOUBLE, G(3) AS DOUBLE
  DIM loss(0) AS DOUBLE, gx(7) AS DOUBLE
  ' dati normalizzati /10: la regressione resta stabile con un passo leggibile
  ' due feature indipendenti: (a,b) = (1,2) (2,3) (3,2) (4,3)
  X(0)=0.1 : X(1)=0.2 : X(2)=0.2 : X(3)=0.3 : X(4)=0.3 : X(5)=0.2 : X(6)=0.4 : X(7)=0.3
  T(0)=0.4 : T(1)=0.7 : T(2)=0.8 : T(3)=1.1

  PRINT "== regressione: y = 2a + b =="
  lh = NEURAL.DENSEINIT(2, 1, 42)
  NEURAL.LOSSMSE(Y, T, 4, 1, G, loss)
  PRINT "  perdita iniziale  = "; FORMAT(loss(0), "#.0000")
  DIM step AS LONG
  FOR step = 1 TO 600
    NEURAL.DENSEFORWARD(lh, X, 4, Y)
    NEURAL.LOSSMSE(Y, T, 4, 1, G, loss)
    NEURAL.DENSEBACKWARD(lh, G, 4, gx)
    NEURAL.DENSESTEP(lh, 0, 0.3)
    NEURAL.DENSEZEROGRAD(lh)
  NEXT step
  NEURAL.DENSEFORWARD(lh, X, 4, Y)
  PRINT "  dopo 600 passi   = "; FORMAT(Y(0) * 10, "#.00"); " "; FORMAT(Y(1) * 10, "#.00"); " "; FORMAT(Y(2) * 10, "#.00"); " "; FORMAT(Y(3) * 10, "#.00")
  NEURAL.LOSSMSE(Y, T, 4, 1, G, loss)
  PRINT "  perdita finale    = "; FORMAT(loss(0), "#.0000")
  DIM W(1) AS DOUBLE, B(0) AS DOUBLE
  NEURAL.DENSEPARAMS(lh, W, B)
  PRINT "  pesi appresi      = "; FORMAT(W(0), "#.00"); " "; FORMAT(W(1), "#.00"); " bias "; FORMAT(B(0), "#.00")

  ' ══ 2) compressione del modello (pruning 2:4, INT8, ternario) ═══════════
  PRINT "== compressione dei pesi =="
  DIM w8(7) AS DOUBLE, q(7) AS LONG, dq(7) AS DOUBLE
  DIM i AS LONG, maxerr AS DOUBLE, e AS DOUBLE
  FOR i = 0 TO 7
    w8(i) = W(0) * (i + 1) / 2
  NEXT i
  PRINT "  pesi azzerati 2:4 = "; NEURAL.SPARSIFY24(w8, 8, 4, 2)
  NEURAL.QUANTI8(w8, 8, 0.05, 0, q)
  NEURAL.DEQUANTI8(q, 8, 0.05, 0, dq)
  maxerr = 0
  FOR i = 0 TO 7
    e = ABS(w8(i) - dq(i))
    IF e > maxerr THEN maxerr = e
  NEXT i
  PRINT "  errore INT8 max   = "; FORMAT(maxerr, "#.0000")
  th = NEURAL.TERNARYINIT(w8, 2, 4)
  DIM tm(7) AS DOUBLE
  NEURAL.TERNARYTOMAT(th, tm)
  PRINT "  pesi ternari     = "; FORMAT(tm(0), "#.00"); " "; FORMAT(tm(1), "#.00"); " "; FORMAT(tm(3), "#.00"); " "; FORMAT(tm(7), "#.00")
  NEURAL.TERNARYFREE(th)

  ' ══ 3) serie temporale: una RNN che impara da un gradiente noto ════════
  PRINT "== serie: RNN tanh =="
  DIM xs(2) AS DOUBLE, ys(2) AS DOUBLE, gy(2) AS DOUBLE
  xs(0)=0.1 : xs(1)=0.2 : xs(2)=0.3
  gy(0)=1 : gy(1)=0 : gy(2)=0
  rh = NEURAL.RNNINIT(1, 2, 1, 7)
  NEURAL.RNNFORWARD(rh, xs, 3, ys)
  PRINT "  prime uscite     = "; FORMAT(ys(0), "#.0000"); " "; FORMAT(ys(1), "#.0000"); " "; FORMAT(ys(2), "#.0000")
  NEURAL.RNNBACKWARD(rh, gy, 3)
  NEURAL.STEP(rh, 2, 0.01)
  NEURAL.ZEROGRAD(rh)
  NEURAL.FREE(rh)

  ' ══ 4) classificazione: albero, foresta, adaboost ═══════════════════════
  PRINT "== classificazione (soglia su una feature) =="
  DIM cx(5) AS DOUBLE, cy(5) AS LONG
  FOR i = 0 TO 5
    cx(i) = i + 1
    IF i < 3 THEN cy(i) = 0 ELSE cy(i) = 1
  NEXT i
  DIM q1(0) AS DOUBLE, q2(0) AS DOUBLE
  q1(0)=2 : q2(0)=5
  th = NEURAL.TREEINIT(3, 2, 1)
  NEURAL.TREEFITCLS(th, cx, cy, 6, 1, 2)
  PRINT "  albero 2 / 5      = "; NEURAL.TREEPREDICT(th, q1); " / "; NEURAL.TREEPREDICT(th, q2)
  NEURAL.TREEFREE(th)
  fh = NEURAL.FORESTINIT(7, 3, 2, 0, 1)
  NEURAL.FORESTFITCLS(fh, cx, cy, 6, 1, 2)
  PRINT "  foresta 2 / 5     = "; NEURAL.FORESTPREDICT(fh, q1); " / "; NEURAL.FORESTPREDICT(fh, q2)
  NEURAL.FORESTFREE(fh)
  ah = NEURAL.ADABOOSTINIT(10, 1.0)
  NEURAL.ADABOOSTFIT(ah, cx, cy, 6, 1)
  PRINT "  adaboost 2 / 5    = "; NEURAL.ADABOOSTPREDICT(ah, q1); " / "; NEURAL.ADABOOSTPREDICT(ah, q2)
  NEURAL.ADABOOSTFREE(ah)

  ' ══ 5) derivata simbolica (autodiff) ═══════════════════════════════════
  PRINT "== autodiff: d/dx (x*y + x) in (2,3) =="
  DIM ha AS LONG, hb AS LONG, hm AS LONG, hz AS LONG
  ha = NEURAL.AUTOVALUE(2)
  hb = NEURAL.AUTOVALUE(3)
  hm = NEURAL.AUTOMUL(ha, hb)
  hz = NEURAL.AUTOADD(hm, ha)
  NEURAL.AUTOBACKWARD(hz)
  PRINT "  valore            = "; NEURAL.AUTODATA(hz)
  PRINT "  d/dx, d/dy        = "; NEURAL.AUTOGRAD(ha); " , "; NEURAL.AUTOGRAD(hb)
  NEURAL.AUTOFREE(hz) : NEURAL.AUTOFREE(hm) : NEURAL.AUTOFREE(ha) : NEURAL.AUTOFREE(hb)

  ' ══ 6) scoperta di una formula con la GP ═══════════════════════════════
  PRINT "== GP: bersaglio x + y*y = 11 (x=2, y=3) =="
  DIM codes(3) AS LONG, arity(3) AS LONG, best(62) AS LONG, gfit(0) AS DOUBLE
  codes(0)=1 : codes(1)=2 : codes(2)=3 : codes(3)=4
  arity(0)=0 : arity(1)=0 : arity(2)=2 : arity(3)=2
  gh = NEURAL.GPINIT(codes, arity, 4)
  n = NEURAL.GPEVOLVE(gh, FormulaFitness, best, gfit, 50, 200, 11, 3)
  PRINT "  errore migliore   = "; FORMAT(-gfit(0), "#.0000")
  PRINT "  genoma            = ";
  FOR i = 0 TO n - 1
    PRINT best(i); " ";
  NEXT i
  PRINT ""
  NEURAL.GPFREE(gh)
  PRINT "NEURAL workbench: fine."
END SUB
