' MiniTest: LLM POLICY GATE <base>[, <ref>] — quantizza base->base.out.gguf con la policy attiva,
' misura col gate (ppl ref vs out) e, se rifiutato, fa ROLLBACK (rimuove l'output). Parita' 4 backend.
' GATE esegue una quantizzazione reale (fixture piccola) -> marcato heavy.
IMPORT LLM
SUB Main()
  LLM POLICY DEFINE "p_gate", "ROLE=ROUTED_EXPERT:q6k;DEFAULT:q4k"
  LLM POLICY APPLY "p_gate"
  LLM POLICY GATE "../../samples/Fixtures/models/tiny_moe.gguf"
  PRINT "gate:done"
END SUB
