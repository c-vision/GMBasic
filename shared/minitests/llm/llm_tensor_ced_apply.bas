' MiniTest: applicazione CED (§4.8) — con CED "on", LLM TENSOR COMPILE esclude i tensori
' ALL-ZERO dal peso (weights_bytes) e lo registra nel campo `ced` dell'artefatto .gmbllm.
' Parità 4 backend (stessa logica del runtime).
IMPORT LLM
SUB Main()
  MKDIR "bin"
  COPY "../../samples/Fixtures/models/tiny_moe.gguf" TO "bin/z.gguf"
  LLM MODEL ABLATE "bin/z.gguf", "bin/z2.gguf", "blk.0.ffn_gate_exps"
  LLM TENSOR CED "bin/z2.gguf", "on"
  LLM TENSOR COMPILE "bin/z2.gguf", "bin/z2.gmbllm"
  PRINT "ced-apply: done"
END SUB
