' ============================================================================
' Qwen3.8 — pipeline di COMPRESSIONE (quant aggressiva + potatura esperti)
' ============================================================================
' OBIETTIVO: da un modello locale >RAM ridurre i pesi di >=30-35% mantenendolo
' usabile, guidando -- in BASIC -- il MODEL COMPILER di GMBasic (LLM).
'
' Il PROFILE dice DOVE sta il peso (misurato su Qwen3.8-Flash-Next-4bit):
'   weights 103.86 GiB · embedding 29.1% · experts 67.8% · attention 1.5% · resto ~1.6%
' => gli ESPERTI (MoE, 512 a 10/tok) sono il lever: quantizzarli/potarli agisce sul ~68%.
'
' PREREQUISITI
'   - modello MLX locale (qui: Qwen3.8-Flash-Next-4bit, arch qwen4_exp, 48 layer)
'   - un DATASET di calibrazione (token .bin o testo) per imatrix/routing:
'     la potatura "usage" e la quant pesata usano il routing REALE, non l'euristica.
'   - RAM/permesso: modello >RAM -> ops in streaming out-of-core (RESIDENCY=SWAP).
'   - eseguire con permesso esplicito (AGENTS: load >RAM) in finestra quieta.
'
' I percorsi sono in VARIABILI stringa (M$ modello, D$ dataset, I$ imatrix, ...):
' i comandi LLM accettano argomenti-path sia letterali sia variabili (fix: bug
' variabile/letterale del parser C, `LLM MODEL ... $var`).
'
' TAG: heavy   (usa un modello grande: escluso dai run di default)
' ============================================================================
IMPORT LLM

SUB Main()
  MKDIR "work"

  ' --- 0. percorsi ----------------------------------------------------------
  M$ = "/Users/gaetano/ai/Models/Qwen3.8-Flash-Next-4bit"   ' <-- il modello (MLX)
  G$ = "work/q38.gguf"                                     ' conversione GGUF (surgery GGUF-only)
  D$ = "work/calib_tokens.bin"                              ' dataset di calibrazione
  I$ = "work/q38.imatrix.gguf"                             ' imatrix prodotto
  P$ = "work/q38_pruned.gguf"                              ' modello potato
  O$ = "work/q38_compressed.gguf"                          ' modello compresso (finale)

  ' --- 0b. CONVERSIONE MLX -> GGUF ------------------------------------------
  ' I verbi di surgery (PRUNE/QUANT) lavorano su GGUF: il checkpoint MLX va convertito.
  ' (LLM MODEL SAVE non converte; LLM MODEL CONVERT <mlx>, <out.gguf>, "GGUF" si'.)
  PRINT "== conversione MLX -> GGUF =="
  LLM MODEL CONVERT M$, G$, "GGUF"

  ' --- 1. BASELINE: quanto pesa, dove, e il costo ---------------------------
  PRINT "== baseline (INSPECT + PROFILE + COST) =="
  LLM MODEL INSPECT M$
  LLM TENSOR PROFILE M$
  LLM TENSOR COST M$

  ' --- 2. CALIBRAZIONE: imatrix + routing reale -----------------------------
  ' Senza calibrazione la potatura e la quant pesata sono cieche: la imatrix
  ' misura l'importanza delle attivazioni, "usage" l'uso REALE degli esperti.
  PRINT "== calibrazione (imatrix/routing) =="
  LLM CALIBRATE G$, D$, I$
  LLM ACTIVATIONSTATS G$, D$

  ' --- 3. POLICY: quant AGGRESSIVA per ruolo + potatura guidata dall'uso -----
  ' Il lever grande e' ROUTED_EXPERT (67.8%): ~2 bit guidato dall'importanza.
  ' Attn/embedding/output restano piu' alti (costano poco, la qualita' ci tiene).
  PRINT "== policy (quant aggressiva + prune usage) =="
  LLM POLICY SET "IMATRIX",       I$
  LLM POLICY SET "ROUTED_EXPERT", "iq2xxs"   ' esperti MoE: ~2.06 bit
  LLM POLICY SET "ATTENTION",     "q8_0"     ' proiezioni attention: ~8.5 bit
  LLM POLICY SET "EMBEDDING",     "q4_k"     ' 29% del peso: 4-bit
  LLM POLICY SET "DEFAULT",       "q4_k"
  LLM POLICY SET "PRUNE_EXPERT",  "usage"    ' pota gli esperti meno usati (routing reale)
  LLM POLICY SHOW

  ' --- 4. POTATURA ESPERTI: da 512 a 384 (~25%) -----------------------------
  PRINT "== prune esperti (512 -> 384) =="
  LLM PRUNE G$, P$, 384

  ' --- 5. QUANTIZZAZIONE (applica la policy attiva) -------------------------
  PRINT "== quant (policy attiva) =="
  LLM QUANT P$, O$

  ' --- 6. VERIFICA: il modello ridotto pesa meno e parla ancora? ------------
  PRINT "== verifica =="
  LLM TENSOR PROFILE O$                                     ' nuovo peso (atteso -30/35%)
  LLM TENSOR LOGITS  O$, "La capitale dell'Italia e'"       ' sanity: genera
  ' gate di qualita' (PPL vs riferimento) se disponibile:
  ' LLM TRAIN EVAL GATE G$, O$, "work/eval_prompts.txt"

  ' --- 7. REGISTRY + provenance ---------------------------------------------
  PRINT "== registry =="
  LLM REGISTRY "ADD", O$, "q38-compressed"
  LLM REGISTRY "TAG", "q38-compressed", "base=qwen3.8-flash-next-4bit;keep_experts=384;quant=iq2xxs"
  LLM REGISTRY "SHOW", "q38-compressed"
  PRINT "== done =="
END SUB
