' MiniTest: LLM TENSOR LOGITS su Falcon-H1-0.5B-Instruct (arch "falcon_h1", ibrido attenzione + Mamba2 + MLP).
' Oracolo: per "The capital of Italy is" il top-6 dev'essere ['"',' the',' a',' Rome',' in',' not'],
' IDENTICO al reference mlx_lm (Falcon-H1 richiede un cache Lista per-layer: m.make_cache()).
' EXPECT: OK
' TAG: heavy
IMPORT LLM

SUB Main()
  ON ERROR GOTO H
  LLM TENSOR LOGITS "%MODELS%/Falcon-H1-0.5B-Instruct-4bit", "The capital of Italy is"
  PRINT "OK"
  GOTO D
H:
  PRINT "ERR"
D:
END SUB
