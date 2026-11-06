' FILE: wsp/analysis.bas
' WSP (FFI) — analisi senza forward su una fixture a verità nota: copy score OV (le DUE definizioni), composizione (Projection Kernel),
' norme per testa, importanza e top-k.
' EXPECT: cs1= 1  1 |cs0= 0 |pk= 4 |pk0= 0 |norms= 4 |top= 2  4 |spec= 4 |bad=-1
IMPORT WSP

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  DIM r AS LONG
  DIM d AS STRING = GMBWORK$ + "/wsp-analysis"
  IF IO.DirExists(GMBWORK$) = 0 THEN MKDIR GMBWORK$
  r = WSP.REMOVEOUTPUT(d)
  MKDIR d
  r = WSP.FIXTURE("induction", d, 3)
  DIM m AS LONG
  m = WSP.LOADMODEL(d)
  DIM s(1) AS DOUBLE
  r = WSP.COPYSCORE(m, 1, 0, "OV", s)
  PRINT "cs1="; MATH.ROUND(s(0), 6); MATH.ROUND(s(1), 6);
  r = WSP.COPYSCORE(m, 0, 0, "OV", s)
  PRINT "|cs0="; MATH.ROUND(s(0), 6);
  PRINT "|pk="; MATH.ROUND(WSP.COMPOSE(m, 0, 0, 1, 0, "k"), 6);
  PRINT "|pk0="; MATH.ROUND(WSP.COMPOSE(m, 0, 0, 1, 0, "q"), 6);
  DIM nrm(3) AS DOUBLE
  PRINT "|norms="; WSP.NORMS(m, 0, "o", nrm, 4);
  DIM sc(4) AS DOUBLE
  sc(0) = 5: sc(1) = 1: sc(2) = 9: sc(3) = 3: sc(4) = 7
  DIM tk(1) AS LONG
  r = WSP.TOPK(sc, 5, 2, tk)
  PRINT "|top="; tk(0); tk(1);
  DIM wr(3) AS DOUBLE
  DIM wi(3) AS DOUBLE
  PRINT "|spec="; WSP.SPECTRUM(m, 1, 0, "OV", wr, wi, 4);
  PRINT "|bad="; WSP.COPYSCORE(m, 9, 0, "OV", s)
  r = WSP.FREEMODEL(m)
  r = WSP.REMOVEOUTPUT(d)
END SUB
