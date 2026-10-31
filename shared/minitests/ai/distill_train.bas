' TRAIN DISTILL TRAIN (Stream D, D2) — wiring delle funzioni di stato su 4 backend.
' Non esegue il training (serve un modello): verifica che LLM.DISTILLLOSS()/HOLD()/DONE()/
' CKPT() siano dispatchate (loss/hold -1, done 0 e ckpt vuoto prima di ogni TRAIN).
' EXPECT: ok: 1|l: -1|h: -1|dn:  0|c:
DIM l AS DOUBLE
DIM h AS DOUBLE
DIM dn AS DOUBLE
DIM c AS STRING
l = LLM.DISTILLLOSS()
h = LLM.DISTILLHOLD()
dn = LLM.DISTILLDONE()
c = LLM.DISTILLCKPT()
IF l = -1 AND h = -1 AND dn = 0 AND c = "" THEN PRINT "ok: 1" ELSE PRINT "ok: 0"
PRINT "l: "; l
PRINT "h: "; h
PRINT "dn: "; dn
PRINT "c: "; c
