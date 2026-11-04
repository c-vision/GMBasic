' MiniTest: TRAIN DISTILL RESTORE (backup inesistente) -> errore catturabile, parità 4 backend.
' EXPECT: caught
IMPORT AI
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  TRAIN DISTILL RESTORE GMBWORK$ + "/no_backup"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
