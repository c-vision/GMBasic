' MiniTest: LLM TENSOR STATE (I7) positivo su fixture -> parità 4 backend.
' EXPECT: # LLM TENSOR STATE '../../samples/Fixtures/models/tiny_moe.gguf' arch=qwen3_moe|phase: prefill+decode (I7)|state=(none) modality=text speculation=NONE kv.dtype=-
IMPORT LLM
SUB Main()
  LLM TENSOR STATE "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
