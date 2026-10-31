' MiniTest: LLM TENSOR IRLEVELS positivo (gerarchia L0-L5 model-aware) -> parità 4 backend.
' EXPECT backfillato dal leg IL.
IMPORT LLM
SUB Main()
  LLM TENSOR IRLEVELS "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
