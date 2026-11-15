' MiniTest: LLM TENSOR EXPOLICY positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR EXPOLICY '../../samples/Fixtures/models/tiny_moe.gguf' experts=2 top_k=1 expert_bytes=0.0 MiB (LLAMA OPTIMIZER)|# policy suggerite: STATIC (fit) | LRU | LFU | RECENCY | PREDICTIVE (router-led)|# router->PREFETCH: ad ogni MOE_LAYER[N], il router attiva top_k=1 esperti -> PREFETCH di QUEI 1|BEFORE MOE_LAYER[N]  (N_BLOCKS_AHEAD=1, dal router della layer N-1 o dalla stessa)|EVICTION=WHEN_PRESSURE, LIFETIME=BLOCK (working-set per blocco), transfer_cost=stream_on_miss|ENGRAM/IDX: i tensori condivisi (shared_experts) restano RESIDENT (lifetime=model)
IMPORT LLM
SUB Main()
  LLM TENSOR EXPOLICY "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
