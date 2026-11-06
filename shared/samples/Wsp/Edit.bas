' Weight-Space Programming — analizzare e MODIFICARE i pesi su un overlay: la base resta immutabile, ogni edit è nel journal,
' si può annullare, e il risultato si scrive solo con MATERIALIZE (che stampa cosa è cambiato).
IMPORT WSP

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  DIM r AS LONG
  DIM dir AS STRING = GMBWORK$ + "/wsp-sample-edit"
  DIM outd AS STRING = GMBWORK$ + "/wsp-sample-edit-out"
  IF IO.DirExists(GMBWORK$) = 0 THEN MKDIR GMBWORK$
  r = WSP.REMOVEOUTPUT(dir): r = WSP.REMOVEOUTPUT(outd)
  MKDIR dir
  r = WSP.FIXTURE("induction", dir, 3)
  DIM m AS LONG
  m = WSP.LOADMODEL(dir)

  ' 1) Analisi senza forward: il layer 1 testa 0 è una testa di induzione (copia); la testa 0 del layer 0 sposta il token.
  DIM s(1) AS DOUBLE
  r = WSP.COPYSCORE(m, 1, 0, "OV", s)
  PRINT "L1H0 copy score:  somma/|somma| = "; MATH.ROUND(s(0), 3); "   frazione positivi = "; MATH.ROUND(s(1), 3)
  PRINT "L0H0 -> L1H0 (K-composition), Projection Kernel = "; MATH.ROUND(WSP.COMPOSE(m, 0, 0, 1, 0, "k"), 3); " su 4"

  ' 2) Edit: si spegne la testa 0 del layer 1 (la copia) e si prova una proiezione di direzione sulla testa 2.
  r = WSP.CHECKPOINT(m, "prima")
  r = WSP.ZEROSLICE(m, "layers.1.attn.o.heads.0")
  PRINT "ZERO  -> "; WSP.LASTREPORT(m)
  DIM dir1(15) AS DOUBLE
  DIM i AS LONG
  FOR i = 0 TO 15
    dir1(i) = 0
  NEXT i
  dir1(0) = 1
  r = WSP.PROJECTOUT(m, "layers.1.attn.o.heads.2", dir1, 1, 16, 0, 1)
  PRINT "PROJECTOUT (norm-preserving) -> "; WSP.LASTREPORT(m)

  ' 3) Journal: cosa è cambiato rispetto alla base, e ritorno al punto salvato.
  PRINT "tensori diversi dalla base: "; WSP.DIFFCOUNT(m); "   edit nel journal: "; WSP.JOURNALPOS(m)
  r = WSP.ROLLBACK(m, "prima")
  PRINT "dopo il rollback: "; WSP.DIFFCOUNT(m); " tensori diversi"

  ' 4) Si rifà l'edit e si scrive il checkpoint nuovo: shard intatti clonati, riepilogo accanto ai pesi.
  r = WSP.ZEROSLICE(m, "layers.1.attn.o.heads.0")
  r = WSP.MATERIALIZE(m, outd)
  PRINT "verifica del checkpoint: "; WSP.VERIFYMODEL(outd, 3); " (0 = ok)"
  PRINT WSP.SUMMARY(m)
  r = WSP.FREEMODEL(m)
  r = WSP.REMOVEOUTPUT(dir): r = WSP.REMOVEOUTPUT(outd)
END SUB
