' MiniTest: LLM TENSOR GEXPORT (DOT) positivo su fixture -> parità 4 backend.
IMPORT LLM
SUB Main()
  MKDIR "work"
  LLM TENSOR GEXPORT "../../samples/Fixtures/models/tiny_moe.gguf", "work/g.dot"
  IF IO.FILEEXISTS("work/g.dot") THEN
    PRINT "gexport: ok"
  ELSE
    PRINT "gexport: FALLITO"
  END IF
END SUB
