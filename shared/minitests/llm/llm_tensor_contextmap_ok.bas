' MiniTest: LLM TENSOR CONTEXTMAP positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR CONTEXTMAP '../../samples/Fixtures/models/tiny_moe.gguf' arch=qwen3_moe ctx=4096 (§4.10 context/semantic, lato modello)|# segmento	token_budget	ruolo	Kv|SYSTEM	256	istruzioni (immutabili, prefix-cache hit)	KV_prefix|TOOL	512	risultati tool (evictable, non-prefisso)	KV_transient|HISTORY	3328	turni utente/assistant (rolling window + GC)	KV_rolling|kv/token=32 B  kv@ctx=0.1 MiB  kv@system=0.0 MiB|CONTEXT_COMMIT: commit del prefisso (SYSTEM+TOOL+parte HISTORY) a ogni turno oppure ogni 512 token;|prefix-cache SHARE su SYSTEM (hit -> salta PREFILL); TRIM/GC su HISTORY (CONTEXTGC); MAP<->KV pages (KVPLAN).|CONTEXT_TRIM: mantieni SYSTEM + ultimi N turni; CONTEXT_GC: evict TOOL + HISTORY vecchia (vedi CONTEXTGC).
IMPORT LLM
SUB Main()
  LLM TENSOR CONTEXTMAP "../../samples/Fixtures/models/tiny_moe.gguf", "4096"
END SUB
