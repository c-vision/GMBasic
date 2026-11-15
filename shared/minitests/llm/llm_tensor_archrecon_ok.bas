' MiniTest: LLM TENSOR ARCHRECON positivo — valida il `.gmbarch` risolto (§4.2) su fixture.
' GMAD eliminato: l'arch viene SOLO dal `.gmbarch`. Parità 4 backend.
' EXPECT: # LLM TENSOR ARCHRECON '../../samples/Fixtures/models/tiny_moe.gguf' (.gmbarch)|.gmbarch : arch=qwen3_moe attention=gqa moe=|modello  : tensori=25|esito    : RICONCILIATO (.gmbarch = fonte unica di architettura)
IMPORT LLM
SUB Main()
  LLM TENSOR ARCHRECON "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
