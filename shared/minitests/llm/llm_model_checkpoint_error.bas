' MiniTest: LLM MODEL CHECKPOINT su src inesistente -> errore CATTURABILE su 4 backend (dispatch ok).
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM MODEL CHECKPOINT GMBWORK$ + "/no.gguf", GMBWORK$ + "/ckpt", "c1"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
