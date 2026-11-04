' LLM CALIBRATE su modello/dataset inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM CALIBRATE GMBWORK$ + "/no_model", GMBWORK$ + "/no_tokens.bin", GMBWORK$ + "/out.imatrix.gguf"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
