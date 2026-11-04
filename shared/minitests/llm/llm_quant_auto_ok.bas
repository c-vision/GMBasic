' MiniTest: LLM QUANT <in>, <out>, <bpw> — auto-allocazione bit per-tensore (4 backend).
' Il terzo argomento (bpw) delega a llmmodelio_model_quantize_auto (stessa sorgente C).
' EXPECT: quant-auto-ok
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  MKDIR GMBWORK$
  LLM QUANT "../../samples/Fixtures/models/tiny_big_moe.gguf", GMBWORK$ + "/auto.gguf", 4.5
  PRINT "quant-auto-ok"
END SUB
