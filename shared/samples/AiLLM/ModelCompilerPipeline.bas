' ModelCompilerPipeline.bas — pipeline COMPLETO del model compiler su una fixture quantizzabile.
' Esercita: POLICY (SETITEM) -> QUANT (ricetta) -> MODEL SAVE -> MODEL DIFF -> CED -> COMPILE
' -> EXPOLICY -> MPLAN -> DECODEPROFILE -> POLICY GATE. Una sola fixture, tutte le fasi.
IMPORT LLM

SUB Main()
  MKDIR "work"
  DIM M AS STRING

  ' --- POLICY: override per-tensore (Livello B) + autopolicy ---
  LLM POLICY CLEARITEM
  LLM POLICY SETITEM "blk.0.ffn_gate_exps.weight", "q6k"
  LLM POLICY DEFINE "pipe", "ROLE=ROUTED_EXPERT:q4k;DEFAULT:q8_0"
  LLM POLICY APPLY "pipe"
  LLM TENSOR AUTOPOLICY "../../Fixtures/models/tiny_big_moe.gguf", "O2"

  ' --- QUANT (applica la ricetta) + SAVE (round-trip) ---
  LLM QUANT "../../Fixtures/models/tiny_big_moe.gguf", "work/q.gguf"
  LLM MODEL SAVE "work/q.gguf", "work/q2.gguf"
  LLM MODEL DIFF "work/q.gguf", "work/q2.gguf", "work/diff.txt"

  ' --- CED (elimination) + COMPILE (.gmbllm) ---
  LLM TENSOR CED "work/q.gguf"
  LLM TENSOR CED "work/q.gguf", "on"
  LLM TENSOR COMPILE "work/q.gguf", "work/q.gmbllm"

  ' --- residenza/streaming: EXPOLICY / MPLAN / DECODEPROFILE ---
  LLM TENSOR EXPOLICY "work/q.gguf", 1
  LLM TENSOR MPLAN "work/q.gguf", 1
  LLM TENSOR DECODEPROFILE "work/q.gguf", "2048:450"

  ' --- GATE (quant->gate->reject/rollback) ---
  LLM POLICY GATE "../../Fixtures/models/tiny_big_moe.gguf"

  LLM POLICY CLEARITEM
  PRINT "pipeline:done"
END SUB
