' MiniTest: LLM MODEL MODULE su file inesistenti -> errore CATTURABILE su tutti i 4 backend (dispatch ok).
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM MODEL MODULE GMBWORK$ + "/no_base.gguf", GMBWORK$ + "/no.gmmod", GMBWORK$ + "/no_mod.gguf", GMBWORK$ + "/out.gguf"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
