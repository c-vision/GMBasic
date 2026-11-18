' MiniTest: LLM TENSOR PROFILE positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR PROFILE '../../samples/Fixtures/models/tiny_moe.gguf' arch=qwen3_moe weights=656 (0.00 GiB)|embedding  0.00 GiB (4.9%)|experts    0.00 GiB (29.3%)|router     0.00 GiB (4.9%)|norm       0.00 GiB (17.1%)|output     0.00 GiB (4.9%)|other      0.00 GiB (39.0%)|kv/token=32 bytes|RESIDENCY: budget=90 GiB -> RESIDENT (tutto in RAM)
IMPORT LLM
SUB Main()
  LLM TENSOR PROFILE "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
