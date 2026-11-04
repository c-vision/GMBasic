' MiniTest: LLM MODEL DIFF su file inesistenti -> errore CATTURABILE su tutti i 4 backend (dispatch ok).
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM MODEL DIFF "work/no_a.gguf", "work/no_b.gguf", "work/rep.txt"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
