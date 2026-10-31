' MiniTest: LLM TENSOR LOGITS su OLMo-Hybrid (allenai/OLMo-Hybrid-7B, arch "olmo_hybrid").
' Hybrid GatedDeltaNet (layer "linear_attention", pre-norm) + GQA senza RoPE
' (layer "full_attention", post-norm). GDN con conv q/k/v separate + allow_neg_eigval +
' gated o_norm (layer_norm_epsilon).
' Oracolo (non in EXPECT): top-8 = mlx_lm/models/olmo_hybrid.py
'   (1, 315, 284, 429, 28, 3, 220, ...), logprob entro ~0.1.
' EXPECT: OK
' TAG: heavy
IMPORT LLM

SUB Main()
  ON ERROR GOTO H
  LLM TENSOR LOGITS "/Users/gaetano/ai/Models/OLMo-Hybrid-7B", "The capital of Italy is"
  PRINT "OK"
  GOTO D
H:
  PRINT "ERR"
D:
END SUB
