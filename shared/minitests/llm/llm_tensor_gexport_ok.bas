' MiniTest: LLM TENSOR GEXPORT (DOT) positivo su fixture -> parità 4 backend.
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  MKDIR GMBWORK$
  LLM TENSOR GEXPORT "../../samples/Fixtures/models/tiny_moe.gguf", GMBWORK$ + "/g.dot"
  IF IO.FILEEXISTS(GMBWORK$ + "/g.dot") THEN
    PRINT "gexport: ok"
  ELSE
    PRINT "gexport: FALLITO"
  END IF
END SUB
