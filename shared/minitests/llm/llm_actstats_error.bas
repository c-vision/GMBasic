' LLM ACTIVATIONSTATS su modello/dataset inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM ACTIVATIONSTATS GMBWORK$ + "/no_model", GMBWORK$ + "/no_tokens.bin"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
