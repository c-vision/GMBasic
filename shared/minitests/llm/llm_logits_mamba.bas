' MiniTest: LLM TENSOR LOGITS su Mamba-130M (arch "mamba", SSM selettivo) su modello REALE.
' Verifica che il forward Mamba carichi e calcoli i logits.
' Oracolo di correttezza (non in EXPECT: richiede il reference): per "The capital of Italy is"
' il top-1 dev'essere { Rome}, IDENTICO al reference mlx_lm (verificato bit-perfect sugli
' intermedi layer-0: embed/norm/convout/xc/silu/B/C/delta/ssm_y/mixer_out).
' EXPECT: OK
' TAG: heavy
IMPORT LLM

SUB Main()
  ON ERROR GOTO H
  LLM TENSOR LOGITS "%MODELS%/mamba-130m-hf-f32", "The capital of Italy is"
  PRINT "OK"
  GOTO D
H:
  PRINT "ERR"
D:
END SUB
