' MiniTest: LLM TENSOR AUTOPOLICY (§4.1) — dal modello + livello qualita' seleziona il formato
' per-ruolo entro il budget e stampa la DSL pronta. Parity 4 backend.
' EXPECT: # LLM TENSOR AUTOPOLICY '../../samples/Fixtures/models/tiny_moe.gguf' arch=qwen3_moe livello=O2 -> budget=4.5 bit/peso|# POLICY DSL (pronta):|LLM POLICY DEFINE "autopolicy_O2", "ROLE=NORM:f16;ROLE=EMBEDDING:f16;ROLE=OUTPUT:q4_k;DEFAULT:q4_k"|# -> APPLY: `LLM POLICY APPLY "autopolicy_O2"`  |  misura/reject: `LLM POLICY GATE <modello>`
IMPORT LLM
SUB Main()
  LLM TENSOR AUTOPOLICY "../../samples/Fixtures/models/tiny_moe.gguf", "O2"
END SUB
