' MiniTest: TRAIN LORA SAVE (namespace TRAIN -> alias di LLM MODEL SAVE) su modello
' inesistente -> errore CATTURABILE. Verifica la parità 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  TRAIN LORA SAVE GMBWORK$ + "/no_model", GMBWORK$ + "/out"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
