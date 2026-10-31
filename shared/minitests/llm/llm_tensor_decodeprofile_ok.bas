' MiniTest: LLM TENSOR DECODEPROFILE positivo su fixture -> parità 4 backend (RISULTATI identici).
IMPORT LLM
SUB Main()
  LLM TENSOR DECODEPROFILE "../../samples/Fixtures/models/tiny_moe.gguf", "4096:450"
END SUB
