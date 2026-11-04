' MiniTest: TRAIN LORA MERGE (namespace TRAIN -> alias di LLM MODEL ADAPTER) su file
' inesistenti -> errore CATTURABILE. Verifica la parità 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  TRAIN LORA MERGE GMBWORK$ + "/no_base", GMBWORK$ + "/no_adapter", GMBWORK$ + "/out"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
