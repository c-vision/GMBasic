' ModelCompilerCallback.bas — Policy Livello B: la GPM invoca una SUB BASIC per-tensore.
' OnDecide(name, role, default) ritorna il formato scelto; il runtime applica gli override.
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
