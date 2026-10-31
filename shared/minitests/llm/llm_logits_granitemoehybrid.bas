' MiniTest: LLM TENSOR LOGITS su Granite-4.0-H-Micro (ibm-granite/granite-4.0-h-micro-4bit,
' arch "granitemoehybrid", variante DENSE). Hybrid Mamba2 (layer "mamba") + GQA senza RoPE
' ("attention"), residuo scalato (residual_multiplier), embedding_multiplier, logits_scaling.
' Oracolo (non in EXPECT): top-8 = mlx_lm/models/granitemoehybrid.py
'   (284, 200, 6, 28, 1151, 11, 319, ...), logprob entro ~0.1.
' EXPECT: OK
' TAG: heavy
IMPORT LLM

SUB Main()
  ON ERROR GOTO H
  LLM TENSOR LOGITS "/Users/gaetano/ai/Models/granite-4.0-h-micro-4bit", "The capital of Italy is"
  PRINT "OK"
  GOTO D
H:
  PRINT "ERR"
D:
END SUB
