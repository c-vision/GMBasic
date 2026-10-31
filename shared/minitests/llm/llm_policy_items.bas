' MiniTest: LLM POLICY ITEMS/ITEM — il PIANO memorizzato per-tensore (da DIFF) e' leggibile
' come lista JSON (ITEMS) e come singolo item per indice (ITEM <i>). Parita' 4 backend.
IMPORT LLM
SUB Main()
  LLM POLICY DEFINE "p_item", "ROLE=ROUTED_EXPERT:q6k;DEFAULT:q4k"
  LLM POLICY APPLY "p_item"
  LLM POLICY DIFF "../../samples/Fixtures/models/tiny_moe.gguf"
  LLM POLICY ITEMS
  LLM POLICY ITEM 0
END SUB
