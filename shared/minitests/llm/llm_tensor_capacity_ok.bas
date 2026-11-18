' MiniTest: LLM TENSOR CAPACITY positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR CAPACITY '../../samples/Fixtures/models/tiny_moe.gguf' arch=qwen3_moe ctx=4096 (§4.9 EXACT CAPACITY + STATIC MEMORY PLAN)|arena[0] weights  = 0.0 MiB (region=model, lifetime=process, align=page)|arena[1] kv       = 0.1 MiB (region=session, lifetime=session, kv/token=32 B)|arena[2] scratch  = 0.1 MiB (region=step, lifetime=step, riuso; O(act+ctx))|EXACT CAPACITY @ctx = 0.3 MiB (weights + kv + scratch)|fit: serve ~0.0 GiB di RAM (device=unified); vedi MPLAN per RESIDENT vs SWAP/SSD
IMPORT LLM
SUB Main()
  LLM TENSOR CAPACITY "../../samples/Fixtures/models/tiny_moe.gguf", "4096"
END SUB
