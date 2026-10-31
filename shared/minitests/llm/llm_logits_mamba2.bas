' MiniTest: LLM TENSOR LOGITS su Mamba2-130M (arch "mamba2", SSD selettivo).
' Oracolo: per "The capital of Italy is" il top-6 dev'essere [' a',' in',' the',' on',' not',' being'],
' IDENTICO al reference mlx_lm (mamba2-130m e' un modello BASE da 130M, non instruct).
' EXPECT: OK
' TAG: heavy
IMPORT LLM

SUB Main()
  ON ERROR GOTO H
  LLM TENSOR LOGITS "/Volumes/Armor5TB/LocalLLM/ai/models/mamba2-130m", "The capital of Italy is"
  PRINT "OK"
  GOTO D
H:
  PRINT "ERR"
D:
END SUB
