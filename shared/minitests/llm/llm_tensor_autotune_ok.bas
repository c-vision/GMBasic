' MiniTest: LLM TENSOR AUTOTUNE (§4.1) — benchmark proiettato O0..O4 + livello raccomandato.
' TAG: heavy — richiede engine MLX/PPL e plugin primmlx; escluso dal default.
IMPORT LLM
SUB Main()
  LLM TENSOR AUTOTUNE "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
