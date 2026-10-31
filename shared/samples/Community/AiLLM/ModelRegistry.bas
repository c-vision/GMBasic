' ModelRegistry.bas — registro / versioning dei modelli (B3).
'
' Superficie: `LLM REGISTRY <op>, <a>, <b>` (alias top-level riscritto dal parser a
' `LLM TENSOR REGISTRY <op>, …`). Esercita OGNI operazione:
'   "CLEAR" / "ADD", <path>, <tag> / "TAG", <tag>, <k=v;…> / "PROMOTE", <tag> /
'   "LIST" / "SHOW", <tag> / "REMOVE", <tag>
' Store persistente: `work/models.registry` (una riga per versione). Deterministico su 4 backend
' (CLEAR all'inizio per azzerare lo stato). Nessun modello reale necessario.
IMPORT LLM
SUB Main()
  PRINT "=== 1. CLEAR (azzera il registro) ==="
  LLM REGISTRY "CLEAR"

  PRINT "=== 2. ADD — registra versioni (path + tag) ==="
  LLM REGISTRY "ADD", "models/qwen4-exp.gguf", "v1"
  LLM REGISTRY "ADD", "models/qwen4-exp-pruned.gguf", "v2-pruned"
  LLM REGISTRY "ADD", "models/qwen4-exp-q4.gguf", "v3-q4"

  PRINT "=== 3. TAG — metriche per versione ==="
  LLM REGISTRY "TAG", "v2-pruned", "ppl=24.12;experts=48"
  LLM REGISTRY "TAG", "v3-q4", "ppl=25.9;size_gb=3.9"

  PRINT "=== 4. PROMOTE — marca la versione corrente ==="
  LLM REGISTRY "PROMOTE", "v3-q4"

  PRINT "=== 5. LIST — elenco versioni ==="
  LLM REGISTRY "LIST"

  PRINT "=== 6. SHOW — dettaglio di una versione ==="
  LLM REGISTRY "SHOW", "v2-pruned"
  LLM REGISTRY "SHOW", "v3-q4"

  PRINT "=== 7. REMOVE — elimina una versione ==="
  LLM REGISTRY "REMOVE", "v1"
  LLM REGISTRY "LIST"
END SUB
