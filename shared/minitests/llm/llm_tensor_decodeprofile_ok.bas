' MiniTest: LLM TENSOR DECODEPROFILE positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR DECODEPROFILE '../../samples/Fixtures/models/tiny_moe.gguf' arch=qwen3_moe ctx=4096 (I3 roofline decode)|weights=0.0 MiB (dense=0.0, experts=0.0, n_exp=2 top_k=1)|per-token read: dense=0.0 MiB + active_experts=0.0 MiB + KV@ctx=0.1 MiB = 0.1 MiB|banda=450 GB/s  -> decode ~3418621.6 tok/s (bound MEMORY; roofline=memory-bound)|ottimizzazioni: kvquant + q8 KV, esperti RESIDENT vs stream (vedi EXPOLICY/MPLAN), speculative decode (SPEC)
IMPORT LLM
SUB Main()
  LLM TENSOR DECODEPROFILE "../../samples/Fixtures/models/tiny_moe.gguf", "4096:450"
END SUB
