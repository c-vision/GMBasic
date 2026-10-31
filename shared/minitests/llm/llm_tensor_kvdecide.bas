' EXPECT: LLM TENSOR KVDECIDE '../../samples/Fixtures/models/tiny_moe.gguf': n_tokens=1000 kv_bytes=4000000000000 -> RECOMPUTE (recompute=1000.0 persist=4000.0 keep=4000000000000.0)
' MiniTest: LLM TENSOR KVDECIDE — reconstruction_cost/policy (§9.A M10a): con banda di ricarica
' che rende il persist piu' caro del ri-prefill -> RECOMPUTE. Parità 4 backend.
IMPORT LLM
SUB Main()
  LLM TENSOR KVDECIDE "../../samples/Fixtures/models/tiny_moe.gguf", "1000,4000000000000"
END SUB
