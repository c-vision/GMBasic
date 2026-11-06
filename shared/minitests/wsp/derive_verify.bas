' FILE: wsp/derive_verify.bas
' WSP (FFI) — chirurgia di architettura (layer, gruppi kv, neuroni), verifica, hash di modello, provenienza e gate.
' EXPECT: reorder= 0 |v1= 0 |prune= 0 |v2= 0 |nh= 2  2  2 |bytes<-1 |ident= 0 |samehash=-1 |badmap=-1 |nowrite= 0 |mat= 1 |v4= 0 |hashchg=-1 |chain= 0 |gate= 1  0  1
IMPORT WSP

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  DIM r AS LONG
  DIM d AS STRING = GMBWORK$ + "/wsp-dv-base"
  DIM o1 AS STRING = GMBWORK$ + "/wsp-dv-reorder"
  DIM o2 AS STRING = GMBWORK$ + "/wsp-dv-prune"
  DIM o3 AS STRING = GMBWORK$ + "/wsp-dv-ident"
  DIM o4 AS STRING = GMBWORK$ + "/wsp-dv-mat"
  IF IO.DirExists(GMBWORK$) = 0 THEN MKDIR GMBWORK$
  r = WSP.REMOVEOUTPUT(d): r = WSP.REMOVEOUTPUT(o1): r = WSP.REMOVEOUTPUT(o2): r = WSP.REMOVEOUTPUT(o3): r = WSP.REMOVEOUTPUT(o4)
  MKDIR d
  r = WSP.FIXTURE("induction", d, 3)
  DIM m AS LONG
  m = WSP.LOADMODEL(d)
  DIM none(0) AS LONG
  REM 1) riordino dei layer [1,0]
  DIM lm(1) AS LONG
  lm(0) = 1: lm(1) = 0
  PRINT "reorder="; WSP.DERIVE(m, o1, lm, 2, none, 0, none, 0);
  PRINT "|v1="; WSP.VERIFYMODEL(o1, 3);
  REM 2) potatura: 2 gruppi kv su 4 e 2 neuroni su 4 per ciascuno dei 2 layer
  DIM kk(3) AS LONG
  kk(0) = 0: kk(1) = 2: kk(2) = 1: kk(3) = 3
  DIM nk(3) AS LONG
  nk(0) = 0: nk(1) = 3: nk(2) = 1: nk(3) = 2
  PRINT "|prune="; WSP.DERIVE(m, o2, none, 0, kk, 2, nk, 2);
  PRINT "|v2="; WSP.VERIFYMODEL(o2, 3);
  DIM m2 AS LONG
  m2 = WSP.LOADMODEL(o2)
  PRINT "|nh="; WSP.DIM(m2, "heads"); WSP.DIM(m2, "kv_heads"); WSP.DIM(m2, "inter");
  PRINT "|bytes<"; (WSP.DIM(m2, "bytes") < WSP.DIM(m, "bytes"));
  r = WSP.FREEMODEL(m2)
  REM 3) identità: stesso hash di modello (tensor-identico)
  PRINT "|ident="; WSP.DERIVE(m, o3, none, 0, none, 0, none, 0);
  m2 = WSP.LOADMODEL(o3)
  PRINT "|samehash="; (WSP.MODELHASH(m2) = WSP.MODELHASH(m));
  r = WSP.FREEMODEL(m2)
  REM 4) parametri non validi: rifiutati PRIMA di scrivere
  DIM bad(1) AS LONG
  bad(0) = 0: bad(1) = 7
  PRINT "|badmap="; WSP.DERIVE(m, GMBWORK$ + "/wsp-dv-bad", bad, 2, none, 0, none, 0);
  PRINT "|nowrite="; IO.DirExists(GMBWORK$ + "/wsp-dv-bad");
  REM 5) edit -> materialize -> verify; hash cambia; provenienza e gate
  r = WSP.SCALESLICE(m, "layers.0.attn.o.heads.1", 2.0)
  PRINT "|mat="; WSP.MATERIALIZE(m, o4);
  PRINT "|v4="; WSP.VERIFYMODEL(o4, 3);
  m2 = WSP.LOADMODEL(o4)
  PRINT "|hashchg="; (WSP.MODELHASH(m2) <> WSP.MODELHASH(m));
  PRINT "|chain="; WSP.PROVENANCE(m, o4, "SCALE layers.0.attn.o.heads.1 2", o4 + "/chain.json");
  r = WSP.FREEMODEL(m2)
  PRINT "|gate="; WSP.GATE(0.02, 0.05, 1); WSP.GATE(0.2, 0.05, 1); WSP.GATE(0.9, 0.8, 0)
  r = WSP.FREEMODEL(m)
  r = WSP.REMOVEOUTPUT(d): r = WSP.REMOVEOUTPUT(o1): r = WSP.REMOVEOUTPUT(o2): r = WSP.REMOVEOUTPUT(o3): r = WSP.REMOVEOUTPUT(o4)
END SUB
