' EXPECT: LLM TENSOR EXECPLAN '../../samples/Fixtures/models/tiny_moe.gguf' -> 'work/plan.json' (arch=qwen3_moe layers=2 weights=656, kv/token=32 skip=0)
' MiniTest: LLM TENSOR EXECPLAN positivo su fixture -> parità 4 backend (params + risultato).
' L'output JSON include l'array "layers" (piano esecutivo per-layer, §9.A M10 EXECUTION PLANNER).
IMPORT LLM
SUB Main()
  CHDIR PROJECTFOLDER()
  MKDIR "work"
  LLM TENSOR EXECPLAN "../../samples/Fixtures/models/tiny_moe.gguf", "work/plan.json"
END SUB
