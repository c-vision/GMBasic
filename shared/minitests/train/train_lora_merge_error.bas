' MiniTest: TRAIN LORA MERGE (namespace TRAIN -> alias di LLM MODEL ADAPTER) su file
' inesistenti -> errore CATTURABILE. Verifica la parità 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  TRAIN LORA MERGE "bin/no_base", "bin/no_adapter", "bin/out"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
