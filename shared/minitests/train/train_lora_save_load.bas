' MiniTest: TRAIN LORA SAVE|LOAD (§4, adapter RAW <-> .safetensors) su ckpt inesistente
' -> errore catturabile, parità 4 backend.
' EXPECT: caught|caught
IMPORT AI
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H1
  TRAIN LORA SAVE GMBWORK$ + "/no_ckpt", GMBWORK$ + "/no_out.safetensors"
  PRINT "no_err1"
  GOTO A2
H1:
  PRINT "caught"
A2:
  ON ERROR GOTO H2
  TRAIN LORA LOAD GMBWORK$ + "/no_adapter.safetensors", GMBWORK$ + "/no_out"
  PRINT "no_err2"
  GOTO D
H2:
  PRINT "caught"
D:
END SUB
