' MiniTest: LLM PROFILE su file inesistente -> errore CATTURABILE su tutti i 4 backend (dispatch ok).
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM PROFILE "work/no.gguf", "work/out.txt"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
