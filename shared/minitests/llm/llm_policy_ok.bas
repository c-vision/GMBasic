' MiniTest: LLM POLICY dichiarativa (SET ruolo,formato + SHOW) -> parità 4 backend.
' Read-only (nessuna scrittura su file). EXPECT backfillato dal leg IL.
' EXPECT: LLM POLICY (role -> formato):|FFN              -> q4_0|ATTENTION        -> q8_0|default: ROUTED_EXPERT->q4k  FFN->q4_0  EMBEDDING->f16  ATTENTION->q8_0  NORM->f32
IMPORT LLM
SUB Main()
  LLM POLICY SET "FFN", "q4_0"
  LLM POLICY SET "ATTENTION", "q8_0"
  LLM POLICY SHOW
END SUB
