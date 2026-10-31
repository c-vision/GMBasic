' MiniTest: CED prova (bounded) su fixture grande — azzera blk.0.ffn_gate_exps, CED lo rileva
' (byte REALI) e COMPILE con kill-switch ON lo esclude. parity4 4 backend.
IMPORT LLM
SUB Main()
  MKDIR "bin"
  LLM MODEL ABLATE "../../samples/Fixtures/models/tiny_big_moe.gguf", "bin/z.gguf", "blk.0.ffn_gate_exps"
  LLM TENSOR CED "bin/z.gguf"
  LLM TENSOR CED "bin/z.gguf", "on"
  LLM TENSOR COMPILE "bin/z.gguf", "bin/z.gmbllm"
END SUB
