' MiniTest: LLM TENSOR LOGITS su dir inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM TENSOR LOGITS GMBWORK$ + "/no_dir", "ping"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
