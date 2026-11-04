' MiniTest: LLM TENSOR INSPECT su file inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM TENSOR INSPECT GMBWORK$ + "/no.gmbllm"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
