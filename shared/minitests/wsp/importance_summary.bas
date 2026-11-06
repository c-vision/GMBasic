' FILE: wsp/importance_summary.bas
' WSP (FFI) — importanza senza dati (baseline sui pesi), documento JSON su file, riepilogo di un overlay vuoto/modificato.
' EXPECT: mlp= 4 |kv= 4 |json=-1 |sum0=-1 |sum1=-1 |lenok=-1
IMPORT WSP

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  DIM r AS LONG
  DIM d AS STRING = GMBWORK$ + "/wsp-importance"
  IF IO.DirExists(GMBWORK$) = 0 THEN MKDIR GMBWORK$
  r = WSP.REMOVEOUTPUT(d)
  MKDIR d
  r = WSP.FIXTURE("induction", d, 3)
  DIM m AS LONG
  m = WSP.LOADMODEL(d)
  DIM sc(3) AS DOUBLE
  PRINT "mlp="; WSP.MLPIMPORTANCE(m, 0, sc, 4);
  PRINT "|kv="; WSP.KVIMPORTANCE(m, 1, sc, 4);
  PRINT "|json="; (WSP.DOCJSON(m, d + "/doc.json") = 0) AND IO.FileExists(d + "/doc.json");
  PRINT "|sum0="; (INSTR(WSP.SUMMARY(m), "TOTALE: 0 tensori") > 0);
  r = WSP.SCALESLICE(m, "layers.0.attn.o.heads.1", 2.0)
  PRINT "|sum1="; (INSTR(WSP.SUMMARY(m), "model.layers.0.self_attn.o_proj.weight") > 0);
  PRINT "|lenok="; (LEN(WSP.SUMMARY(m)) > 80)
  r = WSP.FREEMODEL(m)
  r = WSP.REMOVEOUTPUT(d)
END SUB
