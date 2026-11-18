' MiniTest: LLM POLICY SETITEM — override per-tensore (Livello B core): il codice BASIC/GPM
' decide name->fmt, con priorita' massima sul resto (policy/recipe/ruolo).
' EXPECT: LLM POLICY DIFF '(nessuna policy attiva)' vs DEFAULT — 25 tensori|blk.0.ffn_gate_exps.weight               ROUTED_EXPERT    IQ2_XXS|Q2_K -> q6k|-> 1 tensori cambiati dal default
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  MKDIR GMBWORK$
  LLM POLICY CLEARITEM
  LLM POLICY SETITEM "blk.0.ffn_gate_exps.weight", "q6k"
  LLM POLICY DIFF "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
