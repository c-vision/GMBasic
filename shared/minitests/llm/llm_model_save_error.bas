' MiniTest: LLM MODEL SAVE su file inesistente -> errore CATTURABILE su tutti i 4 backend (dispatch ok).
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM MODEL SAVE "bin/no.gguf", "bin/out.gguf"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
