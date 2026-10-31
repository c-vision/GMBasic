' MiniTest: LLM MODEL REMOVE su file inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM MODEL REMOVE "bin/no.gguf", "bin/no_out.gguf", 1
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
