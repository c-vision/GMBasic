' CompareDense.bas — analisi/surgery per-tensore (B6), ideato dalle utility di antirez/gguf-tools (MIT).
' Esercita DUE comandi:
'   LLM TENSOR DENSE   <moe>, <out>[, <spec-esperti>]  — MoE -> denso estraendo UN esperto per layer
'                                                         (router rimosso, `*_exps` -> ffn_{gate,up,down})
'   LLM TENSOR COMPARE <a>, <b>[, <glob>]              — confronto numerico PER-TENSORE (dequant + rel_l2
'                                                         + avg_diff% normalizzato) con filtro glob
' Scenario: da un MoE si materializza un denso (un esperto per layer), poi si confrontano i modelli
' per isolare cosa cambia (lineage/frozen-layer) e per verificare il breakdown per-ruolo.
' Output deterministico (solo conteggi/rel_l2/avg_diff): eseguibile su tutti i 4 backend.
IMPORT LLM

SUB Main()
  PRINT "=== 1. DENSE — estrai l'esperto 0 per TUTTI i layer (MoE -> denso) ==="
  LLM TENSOR DENSE "../Fixtures/models/tiny_moe.gguf", "work/dense0.gguf", "0"

  PRINT "=== 2. COMPARE — base vs se stesso (deve dare rel_l2=0, avg_diff=0) ==="
  LLM TENSOR COMPARE "../Fixtures/models/tiny_moe.gguf", "../Fixtures/models/tiny_moe.gguf"

  PRINT "=== 3. COMPARE con GLOB — solo i tensori ffn_gate (base vs denso: _exps rinominati) ==="
  LLM TENSOR COMPARE "../Fixtures/models/tiny_moe.gguf", "work/dense0.gguf", "*.ffn_gate*"

  PRINT "=== 4. DENSE con spec PER-LAYER — esperto diverso per blk.0 e blk.1 ==="
  LLM TENSOR DENSE "../Fixtures/models/tiny_moe.gguf", "work/dense01.gguf", "01"

  PRINT "=== 5. COMPARE — due estrazioni diverse (esperto 0 vs 0/1): i gate_exps differiscono ==="
  LLM TENSOR COMPARE "work/dense0.gguf", "work/dense01.gguf", "*.ffn_gate*"
END SUB
