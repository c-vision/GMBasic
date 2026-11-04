' MiniTest: LLM TENSOR INSPECT su file inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM TENSOR INSPECT "work/no.gmbllm"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
