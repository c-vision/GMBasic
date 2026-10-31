' MiniTest — LLM.TOKENS / LLM.DETOKENIZE / LLM.TENSOR e `LLM … INTO` (2026-10-30).
' Il COMANDO stampa (legacy, invariato: vedi llm_tensor_tokenize.bas); la FUNZIONE e la forma
' con INTO restituiscono il valore e NON stampano. Stessa sorgente: dominio C condiviso
' gmb_runtime_c/src/gmb_llm_tok.c → llmmodelio_tokenize_ids_to / _detokenize_to.
' EXPECT: ids=74,71,78,78,81,223,89,81,84,78,70|txt=hello world|into=74,71,78,78,81|out=vocab_size=259
IMPORT LLM
SUB Main()
  DIM ids$ AS STRING
  ids$ = LLM.TOKENS("../../samples/Fixtures/ai/tiny_tokenizer.json", "hello world")
  PRINT "ids=" + ids$
  DIM txt$ AS STRING
  txt$ = LLM.DETOKENIZE("../../samples/Fixtures/ai/tiny_tokenizer.json", "74,71,78,78,81,223,89,81,84,78,70")
  PRINT "txt=" + txt$
  DIM into$ AS STRING
  LLM TOKENS "../../samples/Fixtures/ai/tiny_tokenizer.json", "hello" INTO into$
  PRINT "into=" + into$
  ' forma generica: qualunque sotto-verbo TENSOR, output catturato come valore
  DIM out$ AS STRING
  out$ = LLM.TENSOR("TOKENIZER", "../../samples/Fixtures/ai/tiny_tokenizer.json")
  PRINT IIF(INSTR(out$, "vocab_size=259") > 0, "out=vocab_size=259", "out=KO")
END SUB
