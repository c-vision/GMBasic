' MiniTest: LLM TENSOR MOBJ positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR MOBJ '../../samples/Fixtures/models/tiny_moe.gguf' (M10 memory objects)|{region=weights,  bytes=656, device=gpu, lifetime=model, transfer_cost=one-time(mmap)}|{region=kv,        bytes_per_token=32, device=gpu, lifetime=session, transfer_cost=0}|{region=activation, device=gpu, lifetime=step, transfer_cost=0}|{region=expert_workset, policy=residency, lifecycle=demote/evict LRU}
IMPORT LLM
SUB Main()
  LLM TENSOR MOBJ "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
