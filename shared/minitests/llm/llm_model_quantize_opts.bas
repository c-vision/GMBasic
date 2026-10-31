' MiniTest: LLM MODEL QUANTIZE con argomenti opzionali group/imatrix/expert_bits (G2, 3..6 argomenti).
' Quantizzazione MLX affine su fixture tiny (nessun errore di arità). F16.
' EXPECT: quantize-opts-ok
IMPORT LLM
SUB Main()
  MKDIR "bin"
  LLM MODEL QUANTIZE "../../samples/Fixtures/models/tiny_f16", "bin/q4mlx_opts", 4, 64, "", 6
  PRINT "quantize-opts-ok"
END SUB
