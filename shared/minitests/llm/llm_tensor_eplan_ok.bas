' MiniTest: LLM TENSOR EPLAN positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR EPLAN '../../samples/Fixtures/models/tiny_moe.gguf' experts=2 world=1|rank 0: 2 esperti
IMPORT LLM
SUB Main()
  LLM TENSOR EPLAN "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
