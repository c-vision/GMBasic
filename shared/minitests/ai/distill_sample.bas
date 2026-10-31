' TRAIN DISTILL SAMPLE (Stream D, D1) — wiring delle funzioni di stato su 4 backend.
' Non esegue la generazione (serve un modello): verifica che LLM.DISTILLSAMPLEN()/
' LLM.DISTILLSAMPLEFAIL()/LLM.DISTILLSAMPLEDONE() siano dispatchate (0 prima di ogni SAMPLE).
' EXPECT: ok: 1|n:  0|f:  0|dn:  0
DIM n AS DOUBLE
DIM f AS DOUBLE
DIM dn AS DOUBLE
n = LLM.DISTILLSAMPLEN()
f = LLM.DISTILLSAMPLEFAIL()
dn = LLM.DISTILLSAMPLEDONE()
IF n = 0 AND f = 0 AND dn = 0 THEN PRINT "ok: 1" ELSE PRINT "ok: 0"
PRINT "n: "; n
PRINT "f: "; f
PRINT "dn: "; dn
