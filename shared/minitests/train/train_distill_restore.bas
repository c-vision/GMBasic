' MiniTest: TRAIN DISTILL RESTORE (backup inesistente) -> errore catturabile, parità 4 backend.
' EXPECT: caught
IMPORT AI
SUB Main()
  ON ERROR GOTO H
  TRAIN DISTILL RESTORE "work/no_backup"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
