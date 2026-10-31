' MiniTest: LLM TENSOR MPLAN su file inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM TENSOR MPLAN "work/no.gguf", "5"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
