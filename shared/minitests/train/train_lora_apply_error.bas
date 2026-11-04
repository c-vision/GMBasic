' MiniTest: TRAIN LORA APPLY (namespace TRAIN, F0 -> delega a LLM LORA APPLY) su
' checkpoint inesistente -> errore CATTURABILE. Verifica la parità 4 backend dell'alias.
' TAG: heavy (richiede il motore MLX; escluso dalla parità automatica, GMB_RUN_HEAVY=1)
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  TRAIN LORA APPLY GMBWORK$ + "/no_ckpt"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
