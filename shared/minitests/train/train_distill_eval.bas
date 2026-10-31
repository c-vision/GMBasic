' MiniTest: TRAIN DISTILL EVAL (serve un modello) -> errore catturabile, parità 4 backend.
' TAG: heavy
' EXPECT: caught
IMPORT AI
SUB Main()
  ON ERROR GOTO H
  TRAIN DISTILL EVAL "work/no.jsonl", { "model": "work/no_model", "out": "work/no_ev" }
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
