' MiniTest: LLM PRUNE su file inesistente -> errore CATTURABILE su tutti i 4 backend (dispatch ok).
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM PRUNE GMBWORK$ + "/no.gguf", GMBWORK$ + "/out.gguf", 4
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
