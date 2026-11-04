' MiniTest: LLM TENSOR CAPACITY su file inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM TENSOR CAPACITY GMBWORK$ + "/no.gguf", "8192"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
