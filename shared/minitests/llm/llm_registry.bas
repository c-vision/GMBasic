' LLM REGISTRY — registro/versioning modelli (B3). Alias top-level → LLM TENSOR REGISTRY.
' L'<op> è una stringa ("ADD"/"LIST"/...). Store: work/models.registry (CLEAR per determinismo).
' EXPECT: LLM REGISTRY CLEAR|LLM REGISTRY ADD: tag=v1 path=models/v1.gguf|LLM REGISTRY ADD: tag=v2-q4 path=models/v2-q4.gguf|LLM REGISTRY TAG 'v2-q4': metrics=ppl=8.31;size_gb=3.9|LLM REGISTRY PROMOTE 'v2-q4'|LLM REGISTRY LIST: n=2 tags=v1,v2-q4|LLM REGISTRY SHOW 'v1': path=models/v1.gguf promoted=0 metrics=|LLM REGISTRY SHOW 'v2-q4': path=models/v2-q4.gguf promoted=1 metrics=ppl=8.31;size_gb=3.9|LLM REGISTRY REMOVE 'v1'|LLM REGISTRY LIST: n=1 tags=v2-q4
IMPORT LLM
SUB Main()
  LLM REGISTRY "CLEAR"
  LLM REGISTRY "ADD", "models/v1.gguf", "v1"
  LLM REGISTRY "ADD", "models/v2-q4.gguf", "v2-q4"
  LLM REGISTRY "TAG", "v2-q4", "ppl=8.31;size_gb=3.9"
  LLM REGISTRY "PROMOTE", "v2-q4"
  LLM REGISTRY "LIST"
  LLM REGISTRY "SHOW", "v1"
  LLM REGISTRY "SHOW", "v2-q4"
  LLM REGISTRY "REMOVE", "v1"
  LLM REGISTRY "LIST"
END SUB
