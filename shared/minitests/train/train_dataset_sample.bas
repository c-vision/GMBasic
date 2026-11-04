' MiniTest: TRAIN DATASET SAMPLE (serve un modello) -> errore catturabile, parità 4 backend.
' TAG: heavy
' EXPECT: caught
IMPORT AI
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  TRAIN DATASET SAMPLE GMBWORK$ + "/no.jsonl", { "model": GMBWORK$ + "/no_model", "out": GMBWORK$ + "/no_smp" }
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
