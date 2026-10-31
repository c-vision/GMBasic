' MiniTest: TRAIN DATASET SAMPLE (serve un modello) -> errore catturabile, parità 4 backend.
' TAG: heavy
' EXPECT: caught
IMPORT AI
SUB Main()
  ON ERROR GOTO H
  TRAIN DATASET SAMPLE "bin/no.jsonl", { "model": "bin/no_model", "out": "bin/no_smp" }
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
