' MiniTest: LLM MODEL ADAPTER su file inesistenti -> errore CATTURABILE su tutti i 4 backend (dispatch ok).
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM MODEL ADAPTER GMBWORK$ + "/no_base.gguf", GMBWORK$ + "/no_lora.safetensors", GMBWORK$ + "/out.gguf", "1"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
