' MiniTest: LLM POLICY SETITEM — override per-tensore (Livello B core): il codice BASIC/GPM
' decide name->fmt, con priorita' massima sul resto (policy/recipe/ruolo).
IMPORT LLM
SUB Main()
  MKDIR "bin"
  LLM POLICY CLEARITEM
  LLM POLICY SETITEM "blk.0.ffn_gate_exps.weight", "q6k"
  LLM POLICY DIFF "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
