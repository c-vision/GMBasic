' LLM REGISTRY MEASURE — metriche AUTOMATICHE: size (sempre) + PPL via LLM QUALITY (se si passa
' un prompt/dataset). Su un GGUF l'engine MLX non carica -> `ppl=na` (mai silenzioso). REGISTER->QUALITY.
' EXPECT: LLM REGISTRY CLEAR|LLM REGISTRY ADD: tag=tiny path=../../samples/Fixtures/models/tiny_moe.gguf|LLM REGISTRY MEASURE 'tiny': path=../../samples/Fixtures/models/tiny_moe.gguf metrics=size=2672|LLM REGISTRY MEASURE 'tiny': path=../../samples/Fixtures/models/tiny_moe.gguf metrics=size=2672;ppl=na|LLM REGISTRY SHOW 'tiny': path=../../samples/Fixtures/models/tiny_moe.gguf promoted=0 metrics=size=2672;ppl=na
IMPORT LLM
SUB Main()
  LLM REGISTRY "CLEAR"
  LLM REGISTRY "ADD", "../../samples/Fixtures/models/tiny_moe.gguf", "tiny"
  LLM REGISTRY "MEASURE", "tiny"
  LLM REGISTRY "MEASURE", "tiny", "Hello world"
  LLM REGISTRY "SHOW", "tiny"
END SUB
