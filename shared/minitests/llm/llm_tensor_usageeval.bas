' MiniTest: LLM TENSOR USAGEEVAL — benchmark d'uso S6 (MixBench-style). Dispatch + errore catturabile.
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  MKDIR GMBWORK$
  ON ERROR GOTO H
  LLM TENSOR USAGEEVAL GMBWORK$ + "/no_model.gguf", GMBWORK$ + "/no.tsv"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
