' Weight-Space Programming — sezionare un modello e leggere i suoi pesi, senza eseguirlo.
' Il modello è una fixture a verità nota (2 layer, 4 teste): per un modello vero basta cambiare `dir`
' (es. "~/ai/Models/Qwen3-8B"; famiglie Qwen3/Llama, pesi safetensors).
IMPORT WSP

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  DIM r AS LONG
  DIM dir AS STRING = GMBWORK$ + "/wsp-sample-section"
  IF IO.DirExists(GMBWORK$) = 0 THEN MKDIR GMBWORK$
  r = WSP.REMOVEOUTPUT(dir)
  MKDIR dir
  r = WSP.FIXTURE("induction", dir, 3)

  DIM m AS LONG
  m = WSP.LOADMODEL(dir)
  IF m < 0 THEN
    PRINT "errore: "; WSP.LASTERROR(0)
    EXIT SUB
  END IF
  PRINT "famiglia: "; WSP.MODELTYPE(m); "  layer: "; WSP.DIM(m, "layers"); "  hidden: "; WSP.DIM(m, "hidden")
  PRINT "tensori: "; WSP.TENSORCOUNT(m); "  foglie del documento: "; WSP.LEAFCOUNT(m); "  violazioni: "; WSP.CHECKDOC(m)

  ' L'albero completo: ogni foglia è un tensore con un indirizzo stabile.
  DIM i AS LONG
  FOR i = 0 TO 5
    PRINT "  "; WSP.LEAFADDR(m, i)
  NEXT i

  ' Le partizioni rendono indirizzabili le teste: la testa 1 di o_proj del layer 0 sono 16 x 4 pesi.
  DIM v(63) AS DOUBLE
  PRINT "layers.0.attn.o.heads.1 -> "; WSP.READSLICE(m, "layers.0.attn.o.heads.1", v, 64); " pesi"
  PRINT WSP.TENSORINFO(m, "model.layers.0.self_attn.o_proj.weight")

  ' Il documento canonico (JSON gmb.wsp@1) si salva per ispezionarlo o diffarlo.
  r = WSP.DOCJSON(m, dir + "/wsp-document.json")
  PRINT "documento scritto: "; (r = 0)
  r = WSP.FREEMODEL(m)
  r = WSP.REMOVEOUTPUT(dir)
END SUB
