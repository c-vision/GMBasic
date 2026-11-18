' ModelCompilerCallback.bas — Policy Livello B: la GPM invoca una SUB BASIC per-tensore.
' OnDecide(name, role, default) ritorna il formato scelto; il runtime applica gli override.
' EXPECT: LLM POLICY DIFF '(nessuna policy attiva)' vs DEFAULT — 25 tensori|-> 0 tensori cambiati dal default|--- dopo callback ---|LLM POLICY DIFF '(nessuna policy attiva)' vs DEFAULT — 25 tensori|blk.0.ffn_gate_exps.weight               ROUTED_EXPERT    IQ2_XXS|Q2_K -> q6k|blk.0.ffn_up_exps.weight                 ROUTED_EXPERT    IQ2_XXS|Q2_K -> q6k|blk.0.ffn_down_exps.weight               ROUTED_EXPERT    IQ2_XXS|Q2_K -> q6k|blk.1.ffn_gate_exps.weight               ROUTED_EXPERT    IQ2_XXS|Q2_K -> q6k|blk.1.ffn_up_exps.weight                 ROUTED_EXPERT    IQ2_XXS|Q2_K -> q6k|blk.1.ffn_down_exps.weight               ROUTED_EXPERT    IQ2_XXS|Q2_K -> q6k|-> 6 tensori cambiati dal default
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  MKDIR GMBWORK$
  LLM POLICY CLEARITEM
  LLM POLICY DIFF "../../samples/Fixtures/models/tiny_moe.gguf"
  LLM POLICY CALLBACK OnDecide
  PRINT "--- dopo callback ---"
  LLM POLICY DIFF "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
FUNCTION OnDecide(name AS STRING, role AS STRING, dflt AS STRING) AS STRING
  IF role = "ROUTED_EXPERT" THEN RETURN "q6k"
  RETURN dflt
END FUNCTION
