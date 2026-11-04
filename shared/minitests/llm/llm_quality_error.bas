' MiniTest: TRAIN EVAL su modello inesistente -> errore CATTURABILE su tutti i 4 backend (§4.5 I6).
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  TRAIN EVAL GMBWORK$ + "/no_model", "ciao", GMBWORK$ + "/q.txt"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
