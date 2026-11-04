' MiniTest: LLM MODEL REMOVE su file inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM MODEL REMOVE GMBWORK$ + "/no.gguf", GMBWORK$ + "/no_out.gguf", 1
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
