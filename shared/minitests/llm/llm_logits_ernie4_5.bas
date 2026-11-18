' MiniTest: LLM TENSOR LOGITS su ERNIE-4.5-0.3B-PT-4bit (arch "ernie4_5", dense Llama-like) su modello REALE.
' Verifica che il forward ERNIE carichi e calcoli i logits utilizzando il TOKENIZER SENTENCEPIECE NATIVO
' (il modello ha SOLO tokenizer.model, nessun tokenizer.json): l'engine lo legge via llmtoken_load_sp.
' Oracolo di correttezza (non in EXPECT: richiede il reference): per "The capital of Italy is" il top-8
' dev'essere identico al reference mlx_lm, ordine incluso:
'   {: 0.113} {the 0.086} {located 0.060} {a 0.039} {... 0.033} {? 0.030} {which 0.023} {Italy}
' (ERNIE-4.5 usa RoPE "traditional" (Meta/Llama, rotate_half) e head_dim=128 != hidden/heads: entrambi
'  rispettati dal motore; il top-1 e' ":" NON " Rome" come nei modelli Llama-like standard).
' EXPECT: OK
' TAG: heavy
IMPORT LLM

SUB Main()
  ON ERROR GOTO H
  LLM TENSOR LOGITS "%MODELS%/ERNIE-4.5-0.3B-PT-4bit", "The capital of Italy is"
  PRINT "OK"
  GOTO D
H:
  PRINT "ERR"
D:
END SUB
