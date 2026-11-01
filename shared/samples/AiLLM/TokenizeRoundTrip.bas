' TokenizeRoundTrip.bas — superficie tokenizer: COMANDO (stampa) vs FUNZIONE (valore) e INTO.
'
' Mostra il pattern canonico del linguaggio: il comando stampa (legacy), la funzione restituisce
' il dato; `… INTO var$` assegna senza stampare. Stessa sorgente C per tutti i backend
' (gmb_runtime_c/src/gmb_llm_tok.c → llmmodelio_tokenize_ids_to / _detokenize_to).
'
' Uso: passa il percorso del tokenizer come primo argomento (default: fixture del corpus).
IMPORT LLM

SUB Main()
  DIM model$ AS STRING
  model$ = "../Fixtures/ai/tiny_tokenizer.json"
  IF COMMANDCOUNT() >= 1 THEN model$ = COMMAND$(1)

  DIM testo$ AS STRING
  testo$ = "hello world, tokenizer surface"

  ' 1) COMANDO: stampa n= e gli id (comportamento legacy, invariato)
  PRINT "[comando] (stampa):"
  LLM TOKENS model$, testo$

  ' 2) FUNZIONE: gli id come DATO, niente stampa
  DIM ids$ AS STRING
  ids$ = LLM.TOKENS(model$, testo$)
  PRINT "[funzione] LLM.TOKENS: "; ids$

  ' 3) COMANDO con INTO: assegnazione, niente stampa
  DIM ids2$ AS STRING
  LLM TOKENS model$, testo$ INTO ids2$
  PRINT "[INTO] "; ids2$
  PRINT "[INTO] uguale alla funzione: "; IIF(ids$ = ids2$, "SI", "NO")

  ' 4) conteggio token (funzione) e round-trip detokenize
  PRINT "[token] "; LLM.TOKENCOUNT(model$, testo$)
  DIM ritorno$ AS STRING
  ritorno$ = LLM.DETOKENIZE(model$, ids$)
  PRINT "[round-trip] "; ritorno$
  PRINT "[round-trip] identico: "; IIF(ritorno$ = testo$, "SI", "NO")

  ' 5) forma generica: qualunque sotto-verbo TENSOR, output catturato
  DIM info$ AS STRING
  info$ = LLM.TENSOR("TOKENIZER", model$)
  PRINT "[info] "; info$
END SUB
