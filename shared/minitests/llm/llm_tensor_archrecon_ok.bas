' MiniTest: LLM TENSOR ARCHRECON positivo — valida il `.gmbarch` risolto (§4.2) su fixture.
' GMAD eliminato: l'arch viene SOLO dal `.gmbarch`. Parità 4 backend.
IMPORT LLM
SUB Main()
  LLM TENSOR ARCHRECON "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
