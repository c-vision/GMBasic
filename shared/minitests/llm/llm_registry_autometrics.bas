' LLM REGISTRY ADD — metriche AUTOMATICHE (gap c): la size (byte) del modello è annotata da
' sola se il file/directory esiste (per una DIR: somma ricorsiva dei file — checkpoint MLX/HF);
' resta vuota se il path non esiste (back-compat).
' EXPECT: LLM REGISTRY CLEAR|LLM REGISTRY ADD: tag=tiny path=../../samples/Fixtures/models/tiny_moe.gguf|LLM REGISTRY ADD: tag=dir path=../../samples/Fixtures/models/tiny_f16|LLM REGISTRY ADD: tag=missing path=models/nope.gguf|LLM REGISTRY SHOW 'tiny': path=../../samples/Fixtures/models/tiny_moe.gguf promoted=0 metrics=size=2672|LLM REGISTRY SHOW 'dir': path=../../samples/Fixtures/models/tiny_f16 promoted=0 metrics=size=66675|LLM REGISTRY SHOW 'missing': path=models/nope.gguf promoted=0 metrics=
IMPORT LLM
SUB Main()
  LLM REGISTRY "CLEAR"
  LLM REGISTRY "ADD", "../../samples/Fixtures/models/tiny_moe.gguf", "tiny"
  LLM REGISTRY "ADD", "../../samples/Fixtures/models/tiny_f16", "dir"
  LLM REGISTRY "ADD", "models/nope.gguf", "missing"
  LLM REGISTRY "SHOW", "tiny"
  LLM REGISTRY "SHOW", "dir"
  LLM REGISTRY "SHOW", "missing"
END SUB
