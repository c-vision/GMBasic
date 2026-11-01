' DerivePipeline.bas — compilatore RIPRODUCIBILE (B5): pipeline di pass + delta binario + provenance.
'   DERIVE  (.gmbderive = BASE → PASS… → OUT)            — ricetta riproducibile/auditabile
'   MAKEPATCH / APPLYPATCH (delta binario applicabile)   — storage di cambi sparsi
'   REGISTRY (ricetta legata alla versione)              — provenance nel versioning (B3)
IMPORT LLM
SUB Main()
  PRINT "=== 1. DERIVE — pipeline riproducibile (.gmbderive) ==="
  LLM DERIVE "../Fixtures/models/tiny_ablate.gmbderive"

  PRINT "=== 2. MAKEPATCH / APPLYPATCH — delta binario applicabile ==="
  LLM MAKEPATCH "../Fixtures/models/tiny_moe.gguf", "work/target.gguf", "work/delta.gmbpatch"
  LLM APPLYPATCH "../Fixtures/models/tiny_moe.gguf", "work/delta.gmbpatch", "work/rebuilt.gguf"
  LLM TENSOR QUANTERROR "work/rebuilt.gguf", "work/target.gguf"

  PRINT "=== 3. REGISTRY — versione + ricetta (provenance) ==="
  LLM REGISTRY "CLEAR"
  LLM REGISTRY "ADD", "work/rebuilt.gguf", "v2-ablated"
  LLM REGISTRY "TAG", "v2-ablated", "recipe=tiny_ablate.gmbderive"
  LLM REGISTRY "SHOW", "v2-ablated"
END SUB
