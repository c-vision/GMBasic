' MiniTest: LLM TENSOR DIST positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR DIST '../../samples/Fixtures/models/tiny_moe.gguf' world_size=1|expert-parallel (EP): experts=2 -> 2/rank (byte/rank=0.00 GiB)|tensor-parallel (TP): attn heads=1 kv=1 -> 1/1 head/rank (byte/rank=0.00 GiB)|replicated (other): 0.00 GiB|nota: EP=esperti, TP=teste/KV; all-reduce su residual (TP).
IMPORT LLM
SUB Main()
  LLM TENSOR DIST "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
