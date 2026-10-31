' LLM QUANTERROR — errore di quantizzazione numerico rel-L2 (B4). Alias → LLM TENSOR QUANTERROR.
' ref == cand → rel_l2 = 0. Sui tensori F32/F16 comuni (i quantizzati sono contati).
' EXPECT: LLM TENSOR QUANTERROR '../../samples/Fixtures/models/tiny_moe.gguf' vs '../../samples/Fixtures/models/tiny_moe.gguf': numeric=25 quant=0 only=0 rel_l2=0 max_abs=0
IMPORT LLM
SUB Main()
  LLM QUANTERROR "../../samples/Fixtures/models/tiny_moe.gguf", "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
