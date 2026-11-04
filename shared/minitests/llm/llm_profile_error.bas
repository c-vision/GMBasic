' MiniTest: LLM PROFILE su file inesistente -> errore CATTURABILE su tutti i 4 backend (dispatch ok).
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM PROFILE GMBWORK$ + "/no.gguf", GMBWORK$ + "/out.txt"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
