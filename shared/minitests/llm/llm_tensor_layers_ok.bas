' MiniTest: LLM TENSOR LAYERS positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR LAYERS '../../samples/Fixtures/models/tiny_moe.gguf' arch=qwen3_moe layers=0..1 globals=3|layer 0: 12 tensori|layer 1: 10 tensori
IMPORT LLM
SUB Main()
  LLM TENSOR LAYERS "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
