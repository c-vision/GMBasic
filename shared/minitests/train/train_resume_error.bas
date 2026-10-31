' MiniTest: TRAIN RESUME (namespace TRAIN -> alias di LLM FINETUNE) su modello inesistente
' -> errore CATTURABILE. Verifica la parità 4 backend dell'alias.
' TAG: heavy (richiede il motore MLX + training; escluso di default, GMB_RUN_HEAVY=1)
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  TRAIN RESUME "bin/no_model"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
