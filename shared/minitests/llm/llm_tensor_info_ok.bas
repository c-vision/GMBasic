' MiniTest: LLM TENSOR INFO su fixture (success-case §4.2) — parità 4 backend (cwd per-sample).
' EXPECT: LLM TENSOR INFO '../../samples/Fixtures/models/tiny_moe.gguf': format=GGUF arch=qwen3_moe tensori=25 kv=10 layers=2 modality=text tensor_bytes=656
IMPORT LLM
SUB Main()
  LLM TENSOR INFO "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
