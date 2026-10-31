' MiniTest: LLM TENSOR AUTOPOLICY (§4.1) — dal modello + livello qualita' seleziona il formato
' per-ruolo entro il budget e stampa la DSL pronta. Parity 4 backend.
IMPORT LLM
SUB Main()
  LLM TENSOR AUTOPOLICY "../../samples/Fixtures/models/tiny_moe.gguf", "O2"
END SUB
