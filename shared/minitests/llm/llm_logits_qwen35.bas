' MiniTest: LLM TENSOR LOGITS su Qwen3.8-27B (qwen3_5, ibrido GDN linear_attention + full)
' su modello REALE. Verifica che il forward qwen3_5 carichi e calcoli i logits senza errori.
' Oracolo di correttezza (non in EXPECT: richiede il reference): per "The capital of Italy is"
' il top-1 dev'essere { Rome} (prob ~0.55), IDENTICO al reference mlx_lm. Un prompt NON
' affidabile (es. italiano "La capitale dell'Italia e'") da' entropia alta sia in GMBasic SIA
' nel reference -> non e' un bug dell'engine.
' EXPECT: OK
' TAG: heavy
IMPORT LLM

SUB Main()
  ON ERROR GOTO H
  LLM TENSOR LOGITS "%MODELS%/Qwen3.8-27B-8bit", "The capital of Italy is"
  PRINT "OK"
  GOTO D
H:
  PRINT "ERR"
D:
END SUB
