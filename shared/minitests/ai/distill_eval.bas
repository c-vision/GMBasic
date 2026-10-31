' TRAIN DISTILL EVAL (Stream D, D4+D5) — wiring delle funzioni di stato su 4 backend.
' Non esegue la generazione (serve un modello): verifica che LLM.DISTILLEVALTOK/PASS/ACCEPT(idx)
' siano dispatchate con l'argomento (valgono -1 prima di ogni EVAL).
' EXPECT: ok: 1|b: -1|d: -1|a: -1
DIM b AS DOUBLE
DIM d AS DOUBLE
DIM a AS DOUBLE
b = LLM.DISTILLEVALTOK(0)
d = LLM.DISTILLEVALPASS(1)
a = LLM.DISTILLEVALACCEPT(0)
IF b = -1 AND d = -1 AND a = -1 THEN PRINT "ok: 1" ELSE PRINT "ok: 0"
PRINT "b: "; b
PRINT "d: "; d
PRINT "a: "; a
