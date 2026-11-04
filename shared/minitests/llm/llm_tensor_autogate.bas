' MiniTest: LLM TENSOR AUTOGATE — gate di accettazione QUANT AUTO vs ricetta baseline (6.1/S3). Dispatch + errore catturabile.
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  MKDIR GMBWORK$
  ON ERROR GOTO H
  LLM TENSOR AUTOGATE GMBWORK$ + "/no_model.gguf", GMBWORK$ + "/no.txt"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
