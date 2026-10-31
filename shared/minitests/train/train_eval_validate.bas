' MiniTest: TRAIN EVAL VALIDATE (alias AI EVAL VALIDATE) -> esecuzione + parità 4 backend (folder assente non solleva).
' EXPECT: no_err
IMPORT AI
SUB Main()
  ON ERROR GOTO H
  TRAIN EVAL VALIDATE "risultato" AGAINST "bin/no_vault"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
