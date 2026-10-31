' MiniTest: LLM TENSOR LOGITS su Falcon-Mamba-7B (arch "falcon_mamba", SSM selettivo + mixer_norm).
' Oracolo: per "The capital of Italy is" il top-1 dev'essere { Rome} = reference mlx_lm.
' Nota: falcon_mamba usa use_bcdt_rms=True (mlx_lm lo setta per model_type falcon_mamba):
' mixer_norm = RMSNorm(weightless, eps=1e-6) applicata a delta/B/C prima di dt_proj/scan.
' EXPECT: OK
' TAG: heavy
IMPORT LLM

SUB Main()
  ON ERROR GOTO H
  LLM TENSOR LOGITS "/Volumes/Armor5TB/LocalLLM/ai/models/falcon-mamba-7b-4bit", "The capital of Italy is"
  PRINT "OK"
  GOTO D
H:
  PRINT "ERR"
D:
END SUB
