' MiniTest: LLM TENSOR USAGEEVAL — benchmark d'uso S6 (MixBench-style). Dispatch + errore catturabile.
' EXPECT: caught
IMPORT LLM
SUB Main()
  MKDIR "bin"
  ON ERROR GOTO H
  LLM TENSOR USAGEEVAL "bin/no_model.gguf", "bin/no.tsv"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
