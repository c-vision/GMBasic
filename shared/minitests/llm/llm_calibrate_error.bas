' LLM CALIBRATE su modello/dataset inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM CALIBRATE "bin/no_model", "bin/no_tokens.bin", "bin/out.imatrix.gguf"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
