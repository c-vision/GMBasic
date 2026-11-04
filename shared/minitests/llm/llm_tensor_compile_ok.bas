' MiniTest: LLM TENSOR COMPILE (.gmbllm) positivo su fixture -> parità 4 backend.
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  MKDIR GMBWORK$
  LLM TENSOR COMPILE "../../samples/Fixtures/models/tiny_moe.gguf", GMBWORK$ + "/out.gmbllm"
END SUB
