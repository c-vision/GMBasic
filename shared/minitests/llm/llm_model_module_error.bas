' MiniTest: LLM MODEL MODULE su file inesistenti -> errore CATTURABILE su tutti i 4 backend (dispatch ok).
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM MODEL MODULE "work/no_base.gguf", "work/no.gmmod", "work/no_mod.gguf", "work/out.gguf"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
