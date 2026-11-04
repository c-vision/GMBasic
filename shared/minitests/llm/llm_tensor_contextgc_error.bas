' MiniTest: LLM TENSOR CONTEXTGC su file inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM TENSOR CONTEXTGC GMBWORK$ + "/no.gguf", "8192:4096"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
