' MiniTest: LLM MODEL ROLLBACK su checkpoint inesistente -> errore CATTURABILE su 4 backend (dispatch ok).
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM MODEL ROLLBACK "bin/ckpt", "nope", "bin/out.gguf"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
