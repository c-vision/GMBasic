' MiniTest: TRAIN EVAL su modello inesistente -> errore CATTURABILE su tutti i 4 backend (§4.5 I6).
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  TRAIN EVAL "work/no_model", "ciao", "work/q.txt"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
