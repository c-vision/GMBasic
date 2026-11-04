' MiniTest: TRAIN DISTILL APPLY (applica un adapter) -> errore catturabile, parità 4 backend.
' TAG: heavy
' EXPECT: caught
IMPORT AI
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  TRAIN DISTILL APPLY GMBWORK$ + "/no_ckpt", { "model": GMBWORK$ + "/no_model" }
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
