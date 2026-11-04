' MiniTest: LLM TENSOR COMPILE (.gmbllm) positivo su fixture -> parità 4 backend.
IMPORT LLM
SUB Main()
  MKDIR "work"
  LLM TENSOR COMPILE "../../samples/Fixtures/models/tiny_moe.gguf", "work/out.gmbllm"
END SUB
