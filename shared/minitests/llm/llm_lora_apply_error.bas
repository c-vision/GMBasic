' MiniTest: TRAIN LORA APPLY su checkpoint inesistente -> errore CATTURABILE.
' TAG: heavy (richiede il motore MLX; escluso dalla parità automatica, GMB_RUN_HEAVY=1)
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  TRAIN LORA APPLY "work/no_ckpt"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
