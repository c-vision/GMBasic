' MiniTest: LLM TENSOR TOKENIZE su modello inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM TENSOR TOKENIZE GMBWORK$ + "/no_model", "ciao"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
