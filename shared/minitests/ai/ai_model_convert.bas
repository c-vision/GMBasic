' EXPECT: converted
' MiniTest POSITIVO: LLM MODEL CONVERT su un checkpoint HF/MLX MINIMO (fixture tiny_hf) produce un
' checkpoint GGUF valido (parser + dispatch + converter reale, pochi KB). Non carica il modello.
IMPORT AI WITH ALIAS
IMPORT LLM
SUB Main
  LLM MODEL CONVERT "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/models/tiny_hf", "/tmp/gmbasic-convert-minitest.gguf"
  ' Target "F16": HF -> GGUF a tensori F16 (sorgente per LLM QUANT/auto).
  LLM MODEL CONVERT "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/models/tiny_hf", "/tmp/gmbasic-convert-minitest-f16.gguf", "F16"
  PRINT "converted"
END SUB
