' MiniTest: LLM EXTRACT su file inesistente -> errore CATTURABILE su tutti i 4 backend (dispatch ok).
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM EXTRACT "bin/no.gguf", "bin/out.gguf", 0, 0
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
