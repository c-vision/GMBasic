' LLM.TOKENCOUNT(model, testo) — funzione (B2): numero di token col tokenizer del modello.
' Fonte unica llmtoken (C). Verifica anche che la funzione qualificata sia risolta dal linker.
' EXPECT: count= 11|empty= 0
IMPORT LLM
SUB Main()
  tok$ = "../../samples/Fixtures/ai/tiny_tokenizer.json"
  PRINT "count="; LLM.TOKENCOUNT(tok$, "hello world")
  PRINT "empty="; LLM.TOKENCOUNT(tok$, "")
END SUB
