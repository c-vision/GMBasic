' MiniTest: TRAIN DISTILL SAMPLE (serve un modello) -> errore catturabile, parità 4 backend.
' TAG: heavy
' EXPECT: caught
IMPORT AI
SUB Main()
  ON ERROR GOTO H
  TRAIN DISTILL SAMPLE "work/no_prompts.jsonl", { "model": "work/no_model", "out": "work/no_ds" }
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
