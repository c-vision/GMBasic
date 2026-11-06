' FILE: wsp/edit_overlay.bas
' WSP (FFI) — edit su overlay (la base resta immutabile), report, journal, undo/redo, checkpoint/rollback, proiezione di direzione.
' EXPECT: scale= 0 |rep=toccati=64/64 rel_norm=1 max_abs=0.873504|x2= 2 |changed=-1 |undo= 0 |restored=-1 |redo= 0 |pos= 1  1 |zero= 0 |rb= 0  1 |rbbad=-1 |diff= 1 |proj= 0 |orth= 0 |badaddr=-1 |err=-1
IMPORT WSP

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  DIM r AS LONG
  DIM d AS STRING = GMBWORK$ + "/wsp-edit"
  IF IO.DirExists(GMBWORK$) = 0 THEN MKDIR GMBWORK$
  r = WSP.REMOVEOUTPUT(d)
  MKDIR d
  r = WSP.FIXTURE("induction", d, 3)
  DIM m AS LONG
  m = WSP.LOADMODEL(d)
  DIM T AS STRING = "model.layers.0.self_attn.o_proj.weight"
  DIM h0 AS STRING = WSP.SHA(m, T)
  DIM a(63) AS DOUBLE
  DIM b(63) AS DOUBLE
  r = WSP.READSLICE(m, "layers.0.attn.o.heads.1", a, 64)
  PRINT "scale="; WSP.SCALESLICE(m, "layers.0.attn.o.heads.1", 2.0);
  PRINT "|rep="; WSP.LASTREPORT(m);
  r = WSP.READSLICE(m, "layers.0.attn.o.heads.1", b, 64)
  PRINT "|x2="; MATH.ROUND(b(5) / a(5), 6);
  PRINT "|changed="; (WSP.SHA(m, T) <> h0);
  PRINT "|undo="; WSP.UNDOEDIT(m);
  PRINT "|restored="; (WSP.SHA(m, T) = h0);
  PRINT "|redo="; WSP.REDOEDIT(m);
  PRINT "|pos="; WSP.JOURNALPOS(m); WSP.JOURNALLEN(m);
  r = WSP.CHECKPOINT(m, "uno")
  r = WSP.ZEROSLICE(m, "layers.0.attn.o.heads.3")
  r = WSP.READSLICE(m, "layers.0.attn.o.heads.3", b, 64)
  DIM z AS DOUBLE = 0
  DIM i AS LONG
  FOR i = 0 TO 63
    z = z + ABS(b(i))
  NEXT i
  PRINT "|zero="; MATH.ROUND(z, 6);
  PRINT "|rb="; WSP.ROLLBACK(m, "uno"); WSP.JOURNALPOS(m);
  PRINT "|rbbad="; WSP.ROLLBACK(m, "nessuno");
  PRINT "|diff="; WSP.DIFFCOUNT(m);
  REM proiezione di una direzione: dopo PROJECTOUT le colonne di o_proj (testa 2, layer 1) sono ortogonali a e0
  DIM dir(15) AS DOUBLE
  FOR i = 0 TO 15
    dir(i) = 0
  NEXT i
  dir(0) = 1
  PRINT "|proj="; WSP.PROJECTOUT(m, "layers.1.attn.o.heads.2", dir, 1, 16, 0, 0);
  DIM p(63) AS DOUBLE
  r = WSP.READSLICE(m, "layers.1.attn.o.heads.2", p, 64)
  DIM dp AS DOUBLE = 0
  FOR i = 0 TO 3
    dp = dp + ABS(p(i))
  NEXT i
  PRINT "|orth="; MATH.ROUND(dp, 9);
  PRINT "|badaddr="; WSP.SCALESLICE(m, "layers.9.attn.o.heads.1", 2.0);
  PRINT "|err="; (LEN(WSP.LASTERROR(m)) > 0)
  r = WSP.FREEMODEL(m)
  r = WSP.REMOVEOUTPUT(d)
END SUB
