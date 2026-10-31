' MiniTest: LLM TENSOR IRLEVELS su file inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM TENSOR IRLEVELS "bin/no.gguf"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
