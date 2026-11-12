' MiniTest: LLM TENSOR LOGITS su LFM2-MoE (LiquidAI/LFM2-8B-A1B-4bit, arch "lfm2_moe").
' Verifica che il forward carichi e calcoli i logits. Blocco FFN MoE NUOVO: dense-first
' (num_dense_layers) + router SIGMOID + expert_bias applicato SOLO alla selezione top-k +
' norm_topk_prob/routed_scaling; esperti SwiGLU stacked. Attn riusata da lfm2.
' Oracolo (non in EXPECT: richiede il reference): per "The capital of Italy is" il top-1 deve essere
'   { Rome}  == mlx_lm.
' EXPECT: OK
' TAG: heavy
IMPORT LLM

SUB Main()
  ON ERROR GOTO H
  LLM TENSOR LOGITS "%MODELS%/LFM2-8B-A1B-4bit", "The capital of Italy is"
  PRINT "OK"
  GOTO D
H:
  PRINT "ERR"
D:
END SUB
