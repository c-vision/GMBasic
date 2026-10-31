' MiniTest: CED prova (bounded) su fixture grande — azzera blk.0.ffn_gate_exps, CED lo rileva
' (byte REALI) e COMPILE con kill-switch ON lo esclude. parity4 4 backend.
IMPORT LLM
SUB Main()
  MKDIR "work"
  LLM MODEL ABLATE "../../samples/Fixtures/models/tiny_big_moe.gguf", "work/z.gguf", "blk.0.ffn_gate_exps"
  LLM TENSOR CED "work/z.gguf"
  LLM TENSOR CED "work/z.gguf", "on"
  LLM TENSOR COMPILE "work/z.gguf", "work/z.gmbllm"
END SUB
