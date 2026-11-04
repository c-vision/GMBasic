' MiniTest: TRAIN DISTILL TRAIN (serve un modello) -> errore catturabile, parità 4 backend.
' TAG: heavy
' EXPECT: caught
IMPORT AI
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  TRAIN DISTILL TRAIN GMBWORK$ + "/no_ds.bin", { "model": GMBWORK$ + "/no_model", "out": GMBWORK$ + "/no_dt" }
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
