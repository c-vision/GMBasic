' MiniTest: LLM COMPILE (comando top-level §4.9) su modello inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM COMPILE "work/no_model.gguf", "work/out.gmbllm"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
