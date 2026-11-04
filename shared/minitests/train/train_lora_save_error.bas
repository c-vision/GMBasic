' MiniTest: TRAIN LORA SAVE (namespace TRAIN -> alias di LLM MODEL SAVE) su modello
' inesistente -> errore CATTURABILE. Verifica la parità 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  TRAIN LORA SAVE "work/no_model", "work/out"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
