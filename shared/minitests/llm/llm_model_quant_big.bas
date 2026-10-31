' MiniTest: QUANT -> ricetta (§4.0) su fixture grande (tensori >= 256 elem, quantizzabili).
' SETITEM q6k su blk.0.ffn_gate_exps + TENSOR IR dell'output: i formati rispettano la policy.
IMPORT LLM
SUB Main()
  MKDIR "bin"
  LLM POLICY CLEARITEM
  LLM POLICY SETITEM "blk.0.ffn_gate_exps.weight", "q6k"
  LLM QUANT "../../samples/Fixtures/models/tiny_big_moe.gguf", "bin/q.gguf"
  LLM TENSOR IR "bin/q.gguf"
  LLM POLICY CLEARITEM
END SUB
