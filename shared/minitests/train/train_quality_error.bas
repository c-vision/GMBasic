' MiniTest: TRAIN QUALITY (namespace TRAIN, F0 -> delega a LLM QUALITY) su modello
' inesistente -> errore CATTURABILE. Verifica la parità 4 backend dell'alias TRAIN.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  TRAIN QUALITY "work/no_model", "ciao", "work/q.txt"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
