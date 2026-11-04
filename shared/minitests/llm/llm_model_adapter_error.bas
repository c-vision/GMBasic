' MiniTest: LLM MODEL ADAPTER su file inesistenti -> errore CATTURABILE su tutti i 4 backend (dispatch ok).
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM MODEL ADAPTER "work/no_base.gguf", "work/no_lora.safetensors", "work/out.gguf", "1"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
