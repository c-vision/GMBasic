' MiniTest: TRAIN DISTILL SAMPLE (serve un modello) -> errore catturabile, parità 4 backend.
' TAG: heavy
' EXPECT: caught
IMPORT AI
SUB Main()
  ON ERROR GOTO H
  TRAIN DISTILL SAMPLE "bin/no_prompts.jsonl", { "model": "bin/no_model", "out": "bin/no_ds" }
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
