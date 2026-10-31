' EXPECT: quantized
' MiniTest POSITIVO: LLM MODEL QUANTIZE su un modello F16 MINIMO (fixture tiny_f16) produce una
' dir MLX affine valida (parser + dispatch + tool gmb_model_quantize reale). Non carica il modello.
IMPORT AI WITH ALIAS
IMPORT LLM
SUB Main
  LLM MODEL QUANTIZE "../../samples/Fixtures/models/tiny_f16", "/tmp/gmbasic-quantize-minitest", 4, 64
  PRINT "quantized"
END SUB
