' MiniTest: LLM TENSOR COMPILE (.gmbllm) positivo su fixture -> parità 4 backend.
IMPORT LLM
SUB Main()
  MKDIR "bin"
  LLM TENSOR COMPILE "../../samples/Fixtures/models/tiny_moe.gguf", "bin/out.gmbllm"
END SUB
