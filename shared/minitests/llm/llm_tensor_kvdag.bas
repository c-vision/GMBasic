' EXPECT: LLM TENSOR KVDAG (CSA2): 7 op -> READ v41.base,REUSE v41.reuse.l0,RECONSTRUCT v41.index.l0,REUSE v41.reuse.l1,RECONSTRUCT v41.index.l1,REUSE v41.shared,RECONSTRUCT v41.out
' MiniTest: LLM TENSOR KVDAG — il KV come DAG (base/derived/index) + espansione PREFETCH topologica
' (§9.A M10b). Parità 4 backend (params + risultato).
IMPORT LLM
SUB Main()
  LLM TENSOR KVDAG
END SUB
