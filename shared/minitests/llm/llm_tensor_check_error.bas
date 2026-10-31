' MiniTest: LLM TENSOR CHECK su file inesistente -> errore CATTURABILE su tutti i 4 backend (§4.2).
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM TENSOR CHECK "work/no.gguf"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
