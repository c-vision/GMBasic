' MiniTest: LLM TENSOR INFO su file inesistente -> errore CATTURABILE su tutti i 4 backend (§4.2).
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM TENSOR INFO "bin/no.gguf"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
