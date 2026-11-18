' MiniTest: LLM TENSOR COST positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR COST '../../samples/Fixtures/models/tiny_moe.gguf' arch=qwen3_moe ctx=2048|weights: 656 bytes (0.00 GiB)|kv/token: 32 bytes (0.00 MiB) -> kv@ctx: 0.00 GiB|total@ctx: 0.00 GiB
IMPORT LLM
SUB Main()
  LLM TENSOR COST "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
