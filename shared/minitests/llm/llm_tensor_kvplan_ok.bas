' MiniTest: LLM TENSOR KVPLAN positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR KVPLAN '../../samples/Fixtures/models/tiny_moe.gguf' mode=PAGED ctx=8192 page=256 (§4.6 KV IR PAGE/MAP/UNMAP/PREFETCH/EVICT/SHARE)|kv.dtype=f16  kv/token=32 B  kv@ctx=0.2 MiB  pages=32|PAGE     : 32 pagine da 256 token (block<->asse MAP/UNMAP)|PREFIX   : cache prefissi condivisi (SHARE); hit -> salta PREFILL (si)|PREFETCH : router -> KV (n_blocchi_ahead=1) su miss|EVICT    : LRU su pagine (WHEN_PRESSURE); in RAM|mode scelto=PAGED (FULL|PAGED|ROTATING|DISK_BACKED)
IMPORT LLM
SUB Main()
  LLM TENSOR KVPLAN "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
