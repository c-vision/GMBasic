' MiniTest: LLM MODEL MODULE su file inesistenti -> errore CATTURABILE su tutti i 4 backend (dispatch ok).
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM MODEL MODULE "bin/no_base.gguf", "bin/no.gmmod", "bin/no_mod.gguf", "bin/out.gguf"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
