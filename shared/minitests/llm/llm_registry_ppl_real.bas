' LLM REGISTRY MEASURE — PPL REALE (item 1): su un modello MLX tiny COMMITTATO (tiny_llama, llama
' TAG: heavy — richiede engine MLX/PPL e plugin primmlx; escluso dal default.
' F16 1-layer) l'engine misura una perplexity vera (non `na`). Verifica REGISTER->QUALITY end-to-end.
' Fixture generata da tools/gen_tiny_llama_fixture.py (deterministica).
' EXPECT: LLM REGISTRY CLEAR|LLM REGISTRY ADD: tag=tiny path=../../samples/Fixtures/models/tiny_llama|LLM REGISTRY MEASURE 'tiny': path=../../samples/Fixtures/models/tiny_llama metrics=size=19709;ppl=234.1631|LLM REGISTRY SHOW 'tiny': path=../../samples/Fixtures/models/tiny_llama promoted=0 metrics=size=19709;ppl=234.1631
IMPORT LLM
SUB Main()
  LLM REGISTRY "CLEAR"
  LLM REGISTRY "ADD", "../../samples/Fixtures/models/tiny_llama", "tiny"
  LLM REGISTRY "MEASURE", "tiny", "hello world"
  LLM REGISTRY "SHOW", "tiny"
END SUB
