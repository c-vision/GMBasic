' FILE: wsp/recipe_patch.bas
' WSP (FFI) — il journal come programma: RECIPE/REPLAY riproducono lo stesso stato (sha256); patch sparsa a slice più piccola del tensore.
' EXPECT: journal= 5 |recipe= 0 |replay= 0 |same=-1 |pos= 5 |patch<-1 |apply= 0 |samepatch=-1 |nofile=-1
IMPORT WSP

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  DIM r AS LONG
  DIM d AS STRING = GMBWORK$ + "/wsp-recipe"
  DIM rd AS STRING = GMBWORK$ + "/wsp-recipe-out"
  IF IO.DirExists(GMBWORK$) = 0 THEN MKDIR GMBWORK$
  r = WSP.REMOVEOUTPUT(d): r = WSP.REMOVEOUTPUT(rd)
  MKDIR d
  MKDIR rd
  r = WSP.FIXTURE("induction", d, 3)
  DIM m AS LONG
  m = WSP.LOADMODEL(d)
  DIM dir(15) AS DOUBLE
  DIM i AS LONG
  FOR i = 0 TO 15
    dir(i) = 0
  NEXT i
  dir(1) = 1: dir(2) = 1
  DIM v(63) AS DOUBLE
  FOR i = 0 TO 63
    v(i) = 0.01 * (i + 1)
  NEXT i
  r = WSP.SCALESLICE(m, "layers.0.attn.o.heads.1", 0.5)
  r = WSP.SETVALUES(m, "layers.1.attn.o.heads.0", v, 64)
  r = WSP.ADDDELTA(m, "layers.1.attn.o.heads.0", v, 64)
  r = WSP.PROJECTOUT(m, "layers.1.attn.o.heads.2", dir, 1, 16, 0, 1)
  r = WSP.COPYSLICE(m, "layers.0.attn.q.heads.2", "layers.0.attn.q.heads.0")
  PRINT "journal="; WSP.JOURNALLEN(m);
  PRINT "|recipe="; WSP.SAVERECIPE(m, rd);
  REM stato riprodotto su un modello NUOVO
  DIM m2 AS LONG
  m2 = WSP.LOADMODEL(d)
  PRINT "|replay="; WSP.REPLAYRECIPE(m2, rd);
  DIM same AS LONG = -1
  DIM n AS LONG
  FOR n = 0 TO WSP.TENSORCOUNT(m) - 1
    IF WSP.SHA(m, WSP.TENSORNAME(m, n)) <> WSP.SHA(m2, WSP.TENSORNAME(m, n)) THEN same = 0
  NEXT n
  PRINT "|same="; same;
  PRINT "|pos="; WSP.JOURNALPOS(m2);
  REM patch sparsa: più piccola del tensore intero e riproduce lo stesso stato
  DIM m3 AS LONG
  DIM m4 AS LONG
  m3 = WSP.LOADMODEL(d)
  r = WSP.SCALESLICE(m3, "layers.1.attn.o.heads.0", 2.0)
  DIM pb AS LONG = WSP.EXPORTPATCH(m3, rd + "/p.wsppatch")
  PRINT "|patch<"; (pb > 0 AND pb < 1024);
  m4 = WSP.LOADMODEL(d)
  PRINT "|apply="; WSP.APPLYPATCH(m4, rd + "/p.wsppatch");
  PRINT "|samepatch="; (WSP.SHA(m3, "model.layers.1.self_attn.o_proj.weight") = WSP.SHA(m4, "model.layers.1.self_attn.o_proj.weight"));
  PRINT "|nofile="; WSP.APPLYPATCH(m4, rd + "/nessuna.wsppatch")
  r = WSP.FREEMODEL(m): r = WSP.FREEMODEL(m2): r = WSP.FREEMODEL(m3): r = WSP.FREEMODEL(m4)
  r = WSP.REMOVEOUTPUT(d): r = WSP.REMOVEOUTPUT(rd)
END SUB
