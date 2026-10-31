' MiniTest: TRAIN LORA LOAD (namespace TRAIN -> alias di LLM MODEL LOAD) su modello
' inesistente -> errore CATTURABILE. Verifica la parità 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  TRAIN LORA LOAD "no_model", "work/no_path"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
