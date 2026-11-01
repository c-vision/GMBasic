' TokenizerInspect.bas — superficie tokenizer / chat-template di GMBasic (B2).
'
' Esercita OGNI comando/funzione introdotti (uso realistico, non una sola chiamata):
'   LLM TOKENIZE / DETOKENIZE / VOCAB / TOKENIZER / TEMPLATE / TEMPLATETEST   (alias top-level)
'   n = LLM.TOKENCOUNT(model, testo)                                          (funzione)
' I comandi alias sono riscritti dal parser a `LLM TENSOR <verbo>` (stesso dispatch, 4 backend).
' Deterministico su una fixture tokenizer.json (nessun modello necessario).
IMPORT LLM
SUB Main()
  tok$ = "../Fixtures/ai/tiny_tokenizer.json"
  tpl$ = "../Fixtures/models/tiny_chat"

  PRINT "=== 1. TOKENIZER — fonte + vocab_size ==="
  LLM TOKENIZER tok$

  PRINT "=== 2. VOCAB — vocab_size (senza filtro) ==="
  LLM VOCAB tok$

  PRINT "=== 3. VOCAB — con filtro (argomento opzionale) ==="
  LLM VOCAB tok$, "he"

  PRINT "=== 4. TOKENIZE — testo -> ids ==="
  LLM TOKENIZE tok$, "hello world"

  PRINT "=== 5. DETOKENIZE — ids -> testo (round-trip) ==="
  LLM DETOKENIZE tok$, "74,71,78,78,81,223,89,81,84,78,70"

  PRINT "=== 6. TOKENCOUNT — funzione: numero di token ==="
  PRINT "    'hello world' ->"; LLM.TOKENCOUNT(tok$, "hello world")
  PRINT "    ''            ->"; LLM.TOKENCOUNT(tok$, "")

  PRINT "=== 7. TEMPLATE — mostra il chat template del modello ==="
  LLM TEMPLATE tpl$

  PRINT "=== 8. TEMPLATETEST — rende il template su un transcript ==="
  LLM TEMPLATETEST tpl$, "system:You are helpful|user:Hi"
END SUB
