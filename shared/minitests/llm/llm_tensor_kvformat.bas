' EXPECT: LLM TENSOR KVFORMAT '../../samples/Fixtures/models/tiny_moe.gguf': native=f16 requested=fp4 class=RECONSTRUCTABLE verdict=REJECT
' MiniTest: LLM TENSOR KVFORMAT — contratto GMB_KV_FORMAT (§9.A M10c): fp4-family ammesso SOLO se
' nativo del modello -> rifiutato (mai conversione silenziosa). Parità 4 backend (params + risultato).
IMPORT LLM
SUB Main()
  LLM TENSOR KVFORMAT "../../samples/Fixtures/models/tiny_moe.gguf", "fp4"
END SUB
