' MiniTest: LLM COMPILE (comando top-level §4.9) su modello inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM COMPILE GMBWORK$ + "/no_model.gguf", GMBWORK$ + "/out.gmbllm"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
