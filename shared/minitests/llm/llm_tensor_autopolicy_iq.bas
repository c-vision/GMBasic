' MiniTest: LLM TENSOR AUTOPOLICY livello O6 -> propone IQ1_S (compressione estrema, nuovi formati).
' EXPECT: # LLM TENSOR AUTOPOLICY '../../samples/Fixtures/models/tiny_big_moe.gguf' arch=qwen3_moe livello=O6 -> budget=1.6 bit/peso|# POLICY DSL (pronta):|LLM POLICY DEFINE "autopolicy_O6", "ROLE=NORM:f16;ROLE=EMBEDDING:f16;ROLE=OUTPUT:iq1_s;DEFAULT:iq1_s"|# -> APPLY: `LLM POLICY APPLY "autopolicy_O6"`  |  misura/reject: `LLM POLICY GATE <modello>`
IMPORT LLM
SUB Main()
  LLM TENSOR AUTOPOLICY "../../samples/Fixtures/models/tiny_big_moe.gguf", "O6"
END SUB
