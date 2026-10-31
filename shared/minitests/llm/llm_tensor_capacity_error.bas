' MiniTest: LLM TENSOR CAPACITY su file inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM TENSOR CAPACITY "work/no.gguf", "8192"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
