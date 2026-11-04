' MiniTest: LLM TENSOR DECODEPROFILE su file inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM TENSOR DECODEPROFILE GMBWORK$ + "/no.gguf", "4096:450"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
