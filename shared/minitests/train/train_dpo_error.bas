' MiniTest: TRAIN DPO (preference optimization, F3) su modello inesistente -> errore CATTURABILE.
' TAG: heavy (carica il motore MLX; escluso di default, GMB_RUN_HEAVY=1)
' EXPECT: caught
IMPORT AI
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  TRAIN DPO GMBWORK$ + "/no_model", { "chosen": GMBWORK$ + "/no_c.bin", "rejected": GMBWORK$ + "/no_r.bin" }
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
