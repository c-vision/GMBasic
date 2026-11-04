' MiniTest: firme §4 a 3 argomenti (FINETUNE/DPO) e RESUME <run>,<steps> su modello
' inesistente -> errore catturabile, parità 4 backend.
' TAG: heavy
' EXPECT: caught|caught|caught
IMPORT AI
SUB Main()
  ON ERROR GOTO H1
  TRAIN FINETUNE "work/no_model", "work/no_ds", "work/no_out"
  PRINT "no_err1"
  GOTO A2
H1:
  PRINT "caught"
A2:
  ON ERROR GOTO H2
  TRAIN DPO "work/no_model", "work/no_pairs", "work/no_out"
  PRINT "no_err2"
  GOTO A3
H2:
  PRINT "caught"
A3:
  ON ERROR GOTO H3
  TRAIN RESUME "work/no_model", 5
  PRINT "no_err3"
  GOTO D
H3:
  PRINT "caught"
D:
END SUB
