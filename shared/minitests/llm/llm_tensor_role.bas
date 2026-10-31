' MiniTest: LLM TENSOR ROLE <dir>, <tensor> -> ruolo + formato del singolo tensore.
' EXPECT: LLM TENSOR 'model.layers.0.mlp.switch_mlp.gate_proj.weight': qwen3_moe -> ruolo=ROUTED_EXPERT formato=-
' NOTA (2026-10-29): si usa una fixture con descrittore REALE (tiny_hf, model_type=qwen3_moe):
' la risoluzione legge SOLO la cartella ABI di LLMEnv, non piu' <modello>/arch/*.gmbarch.
IMPORT LLM
SUB Main()
  LLM TENSOR ROLE "../../samples/Fixtures/models/tiny_hf", "model.layers.0.mlp.switch_mlp.gate_proj.weight"
END SUB
