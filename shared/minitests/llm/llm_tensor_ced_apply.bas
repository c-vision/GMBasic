' MiniTest: applicazione CED (§4.8) — con CED "on", LLM TENSOR COMPILE esclude i tensori
' ALL-ZERO dal peso (weights_bytes) e lo registra nel campo `ced` dell'artefatto .gmbllm.
' Parità 4 backend (stessa logica del runtime).
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  MKDIR GMBWORK$
  COPY "../../samples/Fixtures/models/tiny_moe.gguf" TO GMBWORK$ + "/z.gguf"
  LLM MODEL ABLATE GMBWORK$ + "/z.gguf", GMBWORK$ + "/z2.gguf", "blk.0.ffn_gate_exps"
  LLM TENSOR CED GMBWORK$ + "/z2.gguf", "on"
  LLM TENSOR COMPILE GMBWORK$ + "/z2.gguf", GMBWORK$ + "/z2.gmbllm"
  PRINT "ced-apply: done"
END SUB
