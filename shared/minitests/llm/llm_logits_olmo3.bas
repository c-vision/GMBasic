' MiniTest: LLM TENSOR LOGITS su OLMo-3-7B-Instruct (arch "olmo3", dense GQA + q/k-norm flat + POST-norm).
' Oracolo: per "The capital of Italy is" il top-1 dev'essere { Rome} e il top-2 { __},
' IDENTICI al reference mlx_lm (verificato).
' EXPECT: OK
' TAG: heavy
IMPORT LLM

SUB Main()
  ON ERROR GOTO H
  LLM TENSOR LOGITS "/Volumes/Armor5TB/LocalLLM/ai/models/Olmo-3-7B-Instruct", "The capital of Italy is"
  PRINT "OK"
  GOTO D
H:
  PRINT "ERR"
D:
END SUB
