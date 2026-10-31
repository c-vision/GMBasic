' MiniTest: LLM MODEL MODALITY su file inesistenti -> errore CATTURABILE su tutti i 4 backend (dispatch ok).
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM MODEL MODALITY "work/no_base.gguf", "work/no_mmproj.gguf", "work/out.gmmod"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
