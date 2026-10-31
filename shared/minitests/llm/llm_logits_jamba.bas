' MiniTest: LLM TENSOR LOGITS su AI21-Jamba-Reasoning-3B (arch "jamba", ibrido attenzione + Mamba S6, denso).
' Oracolo: per "The capital of Italy is" il top-8 dev'essere ['The',' The','is','the','through','T',',','New'],
' IDENTICO al reference mlx_lm (Jamba richiede un cache Lista per-layer: m.make_cache()).
' EXPECT: OK
' TAG: heavy
IMPORT LLM

SUB Main()
  ON ERROR GOTO H
  LLM TENSOR LOGITS "/Volumes/Armor5TB/LocalLLM/ai/models/AI21-Jamba-Reasoning-3B-4bit", "The capital of Italy is"
  PRINT "OK"
  GOTO D
H:
  PRINT "ERR"
D:
END SUB
