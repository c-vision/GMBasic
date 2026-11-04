' MiniTest: LLM TENSOR COMPILE su file inesistente -> errore CATTURABILE su tutti i 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM TENSOR COMPILE GMBWORK$ + "/no.gguf", GMBWORK$ + "/out.bin"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
