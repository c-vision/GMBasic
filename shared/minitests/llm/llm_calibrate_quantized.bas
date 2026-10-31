' LLM CALIBRATE / ACTIVATIONSTATS su modello QUANTIZZATO (gap a): il forward dequantizza in-memory
' i pesi q* (llmcodec_ggml_dequant_row) -> prima rifiutava i non-F32/F16. Qui: QUANT di tiny_moe,
' poi calibrazione+routing sul modello quantizzato. Output deterministico (parita' 4 backend).
' EXPECT: LLM TENSOR ACTIVATIONSTATS 'bin/tiny_q.gguf': layers=2 experts=2 tokens=16 active=1|LLM CALIBRATE: model=bin/tiny_q.gguf dataset=../../samples/Fixtures/models/tiny_tokens.bin imatrix=bin/calib_q.imatrix.gguf tokens=16
IMPORT LLM
SUB Main()
  MKDIR "bin"
  LLM QUANT "../../samples/Fixtures/models/tiny_moe.gguf", "bin/tiny_q.gguf"
  LLM ACTIVATIONSTATS "bin/tiny_q.gguf", "../../samples/Fixtures/models/tiny_tokens.bin"
  LLM CALIBRATE "bin/tiny_q.gguf", "../../samples/Fixtures/models/tiny_tokens.bin", "bin/calib_q.imatrix.gguf"
END SUB
