' MiniTest: LLM TENSOR KV su file inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM TENSOR KV "bin/no.gguf", "PAGED"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
