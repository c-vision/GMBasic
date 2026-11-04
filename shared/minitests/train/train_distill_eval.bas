' MiniTest: TRAIN DISTILL EVAL (serve un modello) -> errore catturabile, parità 4 backend.
' TAG: heavy
' EXPECT: caught
IMPORT AI
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  TRAIN DISTILL EVAL GMBWORK$ + "/no.jsonl", { "model": GMBWORK$ + "/no_model", "out": GMBWORK$ + "/no_ev" }
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
