' MiniTest: TRAIN DISTILL TRAIN (serve un modello) -> errore catturabile, parità 4 backend.
' TAG: heavy
' EXPECT: caught
IMPORT AI
SUB Main()
  ON ERROR GOTO H
  TRAIN DISTILL TRAIN "bin/no_ds.bin", { "model": "bin/no_model", "out": "bin/no_dt" }
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
