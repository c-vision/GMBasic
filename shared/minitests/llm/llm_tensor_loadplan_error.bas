' MiniTest: LLM TENSOR LOADPLAN su file inesistente -> errore CATTURABILE su tutti i 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM TENSOR LOADPLAN "work/no.gmbllm"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
