' MiniTest: TRAIN DISTILL TRAIN (serve un modello) -> errore catturabile, parità 4 backend.
' TAG: heavy
' EXPECT: caught
IMPORT AI
SUB Main()
  ON ERROR GOTO H
  TRAIN DISTILL TRAIN "work/no_ds.bin", { "model": "work/no_model", "out": "work/no_dt" }
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
