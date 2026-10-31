' MiniTest: TRAIN EVAL (namespace TRAIN, F1 -> alias canonico di LLM QUALITY) su modello
' inesistente -> errore CATTURABILE. Verifica la parità 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  TRAIN EVAL "bin/no_model", "ciao", "bin/q.txt"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
