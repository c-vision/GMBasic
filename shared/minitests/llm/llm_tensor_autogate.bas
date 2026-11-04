' MiniTest: LLM TENSOR AUTOGATE — gate di accettazione QUANT AUTO vs ricetta baseline (6.1/S3). Dispatch + errore catturabile.
' EXPECT: caught
IMPORT LLM
SUB Main()
  MKDIR "work"
  ON ERROR GOTO H
  LLM TENSOR AUTOGATE "work/no_model.gguf", "work/no.txt"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
