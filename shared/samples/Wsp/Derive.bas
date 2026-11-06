' Weight-Space Programming — derivare un modello DIVERSO da uno esistente: togliere/duplicare layer, potare gruppi kv e neuroni.
' Il piano è validato prima di scrivere e il risultato rivalidato dopo. Un derivato è un modello nuovo: la qualità va misurata
' (PPL, KL: vedi la guida WSP), non presunta; l'importanza calcolata sui soli pesi è una baseline debole.
IMPORT WSP

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  DIM r AS LONG
  DIM dir AS STRING = GMBWORK$ + "/wsp-sample-derive"
  DIM o1 AS STRING = GMBWORK$ + "/wsp-sample-derive-1layer"
  DIM o2 AS STRING = GMBWORK$ + "/wsp-sample-derive-pruned"
  IF IO.DirExists(GMBWORK$) = 0 THEN MKDIR GMBWORK$
  r = WSP.REMOVEOUTPUT(dir): r = WSP.REMOVEOUTPUT(o1): r = WSP.REMOVEOUTPUT(o2)
  MKDIR dir
  r = WSP.FIXTURE("induction", dir, 3)
  DIM m AS LONG
  m = WSP.LOADMODEL(dir)
  DIM none(0) AS LONG

  ' (a) un layer in meno: il layer di uscita 0 è il layer sorgente 1
  DIM lm(0) AS LONG
  lm(0) = 1
  r = WSP.DERIVE(m, o1, lm, 1, none, 0, none, 0)
  PRINT "-1 layer: "; WSP.LASTREPORT(m); "   verifica: "; WSP.VERIFYMODEL(o1, 3)

  ' (b) meno neuroni e meno gruppi kv, scelti per importanza (baseline sui pesi) in ogni layer
  DIM sc(3) AS DOUBLE
  DIM gs(3) AS DOUBLE
  DIM kk(3) AS LONG
  DIM nk(3) AS LONG
  DIM pick(1) AS LONG
  DIM l AS LONG
  DIM j AS LONG
  FOR l = 0 TO 1
    r = WSP.MLPIMPORTANCE(m, l, sc, 4)
    r = WSP.TOPK(sc, 4, 2, pick)
    FOR j = 0 TO 1
      nk(l * 2 + j) = pick(j)
    NEXT j
    r = WSP.KVIMPORTANCE(m, l, gs, 4)
    r = WSP.TOPK(gs, 4, 2, pick)
    FOR j = 0 TO 1
      kk(l * 2 + j) = pick(j)
    NEXT j
  NEXT l
  r = WSP.DERIVE(m, o2, none, 0, kk, 2, nk, 2)
  PRINT "potato:  "; WSP.LASTREPORT(m); "   verifica: "; WSP.VERIFYMODEL(o2, 3)

  ' (c) un derivato incoerente viene RIFIUTATO prima di scrivere
  DIM bad(0) AS LONG
  bad(0) = 9
  PRINT "mappa non valida -> "; WSP.DERIVE(m, GMBWORK$ + "/wsp-sample-derive-bad", bad, 1, none, 0, none, 0); " : "; WSP.LASTERROR(m)
  r = WSP.FREEMODEL(m)
  r = WSP.REMOVEOUTPUT(dir): r = WSP.REMOVEOUTPUT(o1): r = WSP.REMOVEOUTPUT(o2)
END SUB
