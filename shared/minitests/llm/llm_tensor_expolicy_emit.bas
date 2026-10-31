' EXPECT: LLM TENSOR EXPOLICY '../../samples/Fixtures/models/tiny_moe.gguf' -> 'bin/expolicy.json' (mode=OFFLOAD hotness=1 prefetch_topn=1 experts=2)
' MiniTest: LLM TENSOR EXPOLICY <path>, <out.json> — emette il PIANO POLICY (formato runtime MLX,
' opzione `expolicy`) — §9.A EXPERT_RESIDENCY_POLICY. Parità 4 backend (params + risultato).
IMPORT LLM
SUB Main()
  MKDIR "bin"
  LLM TENSOR EXPOLICY "../../samples/Fixtures/models/tiny_moe.gguf", "bin/expolicy.json"
END SUB
