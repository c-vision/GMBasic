' MiniTest: TRAIN FINETUNE su modello inesistente -> errore CATTURABILE.
' TAG: heavy (richiede il motore MLX + training; escluso dalla parità automatica, GMB_RUN_HEAVY=1)
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  TRAIN FINETUNE "work/no_model"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
