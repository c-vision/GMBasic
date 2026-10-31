' MiniTest: LLM TENSOR NUMGRAPH su file inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM TENSOR NUMGRAPH "bin/no.gguf"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
