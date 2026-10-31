' MiniTest: LLM QUANT <in>, <out>, <bpw> — auto-allocazione bit per-tensore (4 backend).
' Il terzo argomento (bpw) delega a llmmodelio_model_quantize_auto (stessa sorgente C).
' EXPECT: quant-auto-ok
IMPORT LLM
SUB Main()
  MKDIR "bin"
  LLM QUANT "../../samples/Fixtures/models/tiny_big_moe.gguf", "bin/auto.gguf", 4.5
  PRINT "quant-auto-ok"
END SUB
