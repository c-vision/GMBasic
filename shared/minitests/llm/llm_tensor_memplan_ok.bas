' MiniTest: LLM TENSOR MEMPLAN positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR MEMPLAN '../../samples/Fixtures/models/tiny_moe.gguf' (STATIC MEMORY PLAN / arena)|arena[0] weights  size=656 B (region=model, lifetime=process, align=page)|arena[1] kv       size=kv/token*ctx (region=session, lifetime=session)|arena[2] scratch  size=O(max_act) (region=step, lifetime=step, riuso)
IMPORT LLM
SUB Main()
  LLM TENSOR MEMPLAN "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
