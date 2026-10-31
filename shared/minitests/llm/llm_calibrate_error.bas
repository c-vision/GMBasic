' LLM CALIBRATE su modello/dataset inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM CALIBRATE "work/no_model", "work/no_tokens.bin", "work/out.imatrix.gguf"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
