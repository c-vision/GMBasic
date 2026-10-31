' MiniTest: LLM TENSOR PHASE su file inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM TENSOR PHASE "bin/no.gguf", "1024"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
