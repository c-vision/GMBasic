' MiniTest: LLM TENSOR KV positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR KV '../../samples/Fixtures/models/tiny_moe.gguf' arch=qwen3_moe mode=PAGED (§4.6 KV IR)|KV_FORMAT dtype=f16  activation.dtype=-  state=-|KV_MODE {FULL, PAGED, ROTATING, DISK_BACKED} -> scelto=PAGED|PAGED       : pagine fisse; MAP/UNMAP block<->asse; EVICT LRU; PREFETCH on-miss|KV_FORMAT opzioni: f16 (default) | q8_0 | q4_0 | kvquant (rotazione+tipo); KV_MODE quello sopra
IMPORT LLM
SUB Main()
  LLM TENSOR KV "../../samples/Fixtures/models/tiny_moe.gguf", "PAGED"
END SUB
