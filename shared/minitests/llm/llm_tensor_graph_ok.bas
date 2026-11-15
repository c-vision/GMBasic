' MiniTest: LLM TENSOR GRAPH positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR GRAPH '../../samples/Fixtures/models/tiny_moe.gguf' arch=qwen3_moe attention=gqa moe= layers=0..1|layer 0: moe norm embed/out|layer 1: moe norm
IMPORT LLM
SUB Main()
  LLM TENSOR GRAPH "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
