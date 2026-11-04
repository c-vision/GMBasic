' MiniTest: LLM MODEL MODALITY su file inesistenti -> errore CATTURABILE su tutti i 4 backend (dispatch ok).
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM MODEL MODALITY GMBWORK$ + "/no_base.gguf", GMBWORK$ + "/no_mmproj.gguf", GMBWORK$ + "/out.gmmod"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
