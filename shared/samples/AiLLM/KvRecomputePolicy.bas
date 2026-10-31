' KvRecomputePolicy.bas — §9.A M10a: reconstruction_cost / policy RECOMPUTE vs PERSIST.
'
' Un oggetto KV può essere TRATTENUTO (KEEP), PERSISTITO (su disco) o SCARTATO e RICOSTRUITO
' (ri-prefill). La policy sceglie il minimo tra i costi:
'   COST_RECOMPUTE = n_tokens * costo/token ;  COST_PERSIST = kv_bytes / banda_ricarica ;
'   COST_KEEP = costo_per_byte_residenza * kv_bytes.
' `LLM TENSOR KVDECIDE <model>, "<n_tokens>,<kv_bytes>"` stampa la decisione dal modello di costo
' dell'engine (llm_mlx_kvcost). Con banda di ricarica bassa il ri-prefill conviene -> RECOMPUTE.
IMPORT LLM

SUB Main()
  DIM model AS STRING = "work/modello.gguf"

  ' KV grande (4 TB da ricaricare) vs 1000 token di ri-prefill -> RECOMPUTE.
  LLM TENSOR KVDECIDE model, "1000,4000000000000"
  ' KV piccolo (10 MB) e banda alta -> PERSIST conveniente.
  LLM TENSOR KVDECIDE model, "1000,10000000"
  PRINT "done"
END SUB
