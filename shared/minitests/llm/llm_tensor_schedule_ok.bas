' MiniTest: LLM TENSOR SCHEDULE positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR SCHEDULE '../../samples/Fixtures/models/tiny_moe.gguf' seqlen=2048 weights=0.00 GiB|kv/seq=65536 bytes (0.06 MiB)  max_batch(kv_bound)=1441791  -> CONTINUOUS BATCHING (paged KV, prefill/decode mixed)
IMPORT LLM
SUB Main()
  LLM TENSOR SCHEDULE "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
