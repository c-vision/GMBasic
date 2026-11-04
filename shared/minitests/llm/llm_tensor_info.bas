' MiniTest: LLM TENSOR INFO su file inesistente -> errore CATTURABILE su tutti i 4 backend (§4.2).
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM TENSOR INFO GMBWORK$ + "/no.gguf"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
