' KvDagPrefetch.bas — §9.A M10b: il KV come DAG (KV_BASE · KV_DERIVED · KV_INDEX).
'
' In CSA2 un layer può RIUSARE il KV di un altro (FULL/REINDEX/REUSE): un oggetto memoria non è
' "il KV del layer X" ma un NODO con producer/consumers/dipendenze. Il PREFETCH di un oggetto si
' ESPANDE in ordine TOPOLOGICO: READ (base) | REUSE (derivato condiviso) | RECONSTRUCT (reindex).
' `LLM TENSOR KVDAG[, <out.json>]` mostra l'espansione del grafo CSA2 rappresentativo (engine
' `llm_mlx_kvdag`); l'ordine di dipendenza coincide con quello del forward.
IMPORT LLM

SUB Main()
  ' Espansione PREFETCH del DAG KV (base/derived/index) — ordine topologico.
  LLM TENSOR KVDAG

  ' Serializzazione opzionale del grafo+espansione.
  LLM TENSOR KVDAG, "work/kvdag.json"
  PRINT "grafo KV serializzato: work/kvdag.json"
END SUB
