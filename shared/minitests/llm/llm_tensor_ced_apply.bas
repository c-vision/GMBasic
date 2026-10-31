' MiniTest: applicazione CED (§4.8) — con CED "on", LLM TENSOR COMPILE esclude i tensori
' ALL-ZERO dal peso (weights_bytes) e lo registra nel campo `ced` dell'artefatto .gmbllm.
' Parità 4 backend (stessa logica del runtime).
IMPORT LLM
SUB Main()
  MKDIR "work"
  COPY "../../samples/Fixtures/models/tiny_moe.gguf" TO "work/z.gguf"
  LLM MODEL ABLATE "work/z.gguf", "work/z2.gguf", "blk.0.ffn_gate_exps"
  LLM TENSOR CED "work/z2.gguf", "on"
  LLM TENSOR COMPILE "work/z2.gguf", "work/z2.gmbllm"
  PRINT "ced-apply: done"
END SUB
