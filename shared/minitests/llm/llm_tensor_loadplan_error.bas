' MiniTest: LLM TENSOR LOADPLAN su file inesistente -> errore CATTURABILE su tutti i 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM TENSOR LOADPLAN GMBWORK$ + "/no.gmbllm"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
