' FILE: wsp/model_doc.bas
' WSP (FFI) — apertura di un modello, documento gmb.wsp@1, dimensioni, indirizzi e lettura di slice.
' EXPECT: fx= 0 |type=qwen3|doc= 1 |L= 2 |H= 16 |nh= 4 |kv= 4 |hd= 4 |I= 4 |tens= 20 |leaves= 20 |check= 0 |info=F32 [16,16] 1024 byte|leaf0=embed|n= 64 |sha= 64 |bad=-1
IMPORT WSP

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  DIM r AS LONG
  DIM d AS STRING = GMBWORK$ + "/wsp-model-doc"
  IF IO.DirExists(GMBWORK$) = 0 THEN MKDIR GMBWORK$
  r = WSP.REMOVEOUTPUT(d)
  MKDIR d
  PRINT "fx="; WSP.FIXTURE("induction", d, 3);
  DIM m AS LONG
  m = WSP.LOADMODEL(d)
  PRINT "|type="; WSP.MODELTYPE(m);
  PRINT "|doc="; WSP.HASDOC(m);
  PRINT "|L="; WSP.DIM(m, "layers");
  PRINT "|H="; WSP.DIM(m, "hidden");
  PRINT "|nh="; WSP.DIM(m, "heads");
  PRINT "|kv="; WSP.DIM(m, "kv_heads");
  PRINT "|hd="; WSP.DIM(m, "head_dim");
  PRINT "|I="; WSP.DIM(m, "inter");
  PRINT "|tens="; WSP.TENSORCOUNT(m);
  PRINT "|leaves="; WSP.LEAFCOUNT(m);
  PRINT "|check="; WSP.CHECKDOC(m);
  PRINT "|info="; WSP.TENSORINFO(m, "model.layers.0.self_attn.q_proj.weight");
  PRINT "|leaf0="; WSP.LEAFADDR(m, 0);
  DIM v(255) AS DOUBLE
  PRINT "|n="; WSP.READSLICE(m, "layers.0.attn.o.heads.0", v, 256);
  PRINT "|sha="; LEN(WSP.SHA(m, "model.norm.weight"));
  PRINT "|bad="; WSP.READSLICE(m, "layers.9.attn.o.heads.0", v, 256)
  r = WSP.FREEMODEL(m)
  r = WSP.REMOVEOUTPUT(d)
END SUB
