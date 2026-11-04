' MiniTest: TRAIN DISTILL APPLY (applica un adapter) -> errore catturabile, parità 4 backend.
' TAG: heavy
' EXPECT: caught
IMPORT AI
SUB Main()
  ON ERROR GOTO H
  TRAIN DISTILL APPLY "work/no_ckpt", { "model": "work/no_model" }
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
