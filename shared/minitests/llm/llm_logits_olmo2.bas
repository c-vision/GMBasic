' MiniTest: LLM TENSOR LOGITS su OLMo-2-0425-1B-Instruct (arch "olmo2", dense GQA + q/k-norm flat + POST-norm).
' Verifica che il forward OLMo-2 carichi e calcoli i logits. Il blocco riusa olmo3_attention_block/olmo3_ffn_block
' (stesso schema POST-norm + q/k-norm flat), SENZA la sliding-window di OLMo-3 (full causal).
' Oracolo di correttezza (non in EXPECT: richiede il reference): per "The capital of Italy is" il top-8
' dev'essere identico al reference mlx_lm, ordine incluso:
'   { Rome 0.905} { Florence} { Milan} { Naples} { Tur} { Roma} { the} { a}
' EXPECT: OK
' TAG: heavy
IMPORT LLM

SUB Main()
  ON ERROR GOTO H
  LLM TENSOR LOGITS "%MODELS%/OLMo-2-0425-1B-Instruct", "The capital of Italy is"
  PRINT "OK"
  GOTO D
H:
  PRINT "ERR"
D:
END SUB
