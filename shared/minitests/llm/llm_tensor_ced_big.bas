' MiniTest: CED prova (bounded) su fixture grande — azzera blk.0.ffn_gate_exps, CED lo rileva
' (byte REALI) e COMPILE con kill-switch ON lo esclude. parity4 4 backend.
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  MKDIR GMBWORK$
  LLM MODEL ABLATE "../../samples/Fixtures/models/tiny_big_moe.gguf", GMBWORK$ + "/z.gguf", "blk.0.ffn_gate_exps"
  LLM TENSOR CED GMBWORK$ + "/z.gguf"
  LLM TENSOR CED GMBWORK$ + "/z.gguf", "on"
  LLM TENSOR COMPILE GMBWORK$ + "/z.gguf", GMBWORK$ + "/z.gmbllm"
END SUB
