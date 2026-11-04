' MiniTest: LLM MODEL CHECKPOINT su src inesistente -> errore CATTURABILE su 4 backend (dispatch ok).
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM MODEL CHECKPOINT "work/no.gguf", "work/ckpt", "c1"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
