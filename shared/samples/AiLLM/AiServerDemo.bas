' PARITY-EXCLUDE: carica un modello LOCALE; solo manuale (sentinella).
'' FILE: Ai/AiServerDemo.bas
' =================================================================================================
' AiServerDemo — comando LLM SERVER (gestione del server LLM OpenAI-compatible come processo
' esterno) + client BASIC via FETCH.
'
' Ciclo dimostrato:
'   LLM SERVER START <modelDir> [port] avvia il server C (gmb_llm_server) come processo, porta
'                                   opzionale (default 8020), e attende che sia pronto.
'   NOTA fail-loud: su errore (es. model dir inesistente) il comando alza un errore visibile.

'   GET  /v1/status  (FETCH)        stato live (loaded/busy/model).
'   POST /v1/chat/completions       domanda via chat completions (non-stream).
'   LLM SERVER STOP                  ferma il processo.
'
' Richiede: build del server (native/gmb_llm_server) individuata da GMBASIC_LLM_SERVER o
' native/gmb_llm_server/build; modello MLX presente (cambiare MODEL_DIR$ se serve).
' NB: i body JSON sono letterali unici per riga (iniziano con `{` e finiscono con `}`) perche'
' in GM-BASIC tre virgolette consecutive aprono una raw string.
' =================================================================================================
IMPORT AI WITH ALIAS
IMPORT LLM
IMPORT HTTP
IMPORT JSON

SUB Main
  DIM MODEL_DIR$ = "~/ai/Models/Llama-3.2-1B-Instruct-4bit"
  DIM BASE_URL$ = "http://127.0.0.1:8020"

  IF NOT FileExists(MODEL_DIR$ + "/config.json") THEN
    PRINT "Modello non trovato: "; MODEL_DIR$
    PRINT "Modifica MODEL_DIR$ in cima al file e riesegui."
    END
  END IF

  PRINT "1) LLM SERVER START "; MODEL_DIR$
  LLM SERVER START MODEL_DIR$
  PRINT "   server avviato (readiness ok)."

  PRINT "1b) LLM SERVER STATUS (comando: stato live, fail-loud su errore)"
  LLM SERVER STATUS
  PRINT "   status comando ok."

  PRINT "2) GET /v1/status via FETCH"
  DIM st AS FETCHRESULT = FETCH(BASE_URL$ + "/v1/status")
  IF st.IsSuccess THEN
    DIM sj AS JSON = JSON.PARSE(st.Body)
    PRINT "   loaded="; sj("loaded"); " busy="; sj("busy"); " model="; sj("model")
  ELSE
    PRINT "   status fallito: "; st.StatusCode
  END IF

  PRINT "3) POST /v1/chat/completions"
  DIM co AS HTTPOPTIONS = NEW HTTPOPTIONS
  co.Method = "POST"
  co.Headers.ContentType = "application/json"
  co.Timeout = 30000
  co.Body = "{""model"":""Llama-3.2-1B-Instruct-4bit"",""max_tokens"":24,""temperature"":1,""messages"":[{""role"":""user"",""content"":""Name the capital of France.""}]}"
  DIM chat AS FETCHRESULT = FETCH(BASE_URL$ + "/v1/chat/completions", co)
  IF chat.IsSuccess THEN
    DIM cj AS JSON = JSON.PARSE(chat.Body)
    PRINT "   risposta: "; cj("choices")(0)("message")("content")
  ELSE
    PRINT "   chat fallita: "; chat.StatusCode
  END IF

  PRINT "4) residenza: LLM SERVER UNLOAD / LLM SERVER MODEL (swap)"
  LLM SERVER UNLOAD
  DIM st2 AS FETCHRESULT = FETCH(BASE_URL$ + "/v1/status")
  IF st2.IsSuccess THEN
    DIM sj2 AS JSON = JSON.PARSE(st2.Body)
    PRINT "   loaded dopo UNLOAD="; sj2("loaded")
  END IF
  LLM SERVER MODEL MODEL_DIR$
  DIM st3 AS FETCHRESULT = FETCH(BASE_URL$ + "/v1/status")
  IF st3.IsSuccess THEN
    DIM sj3 AS JSON = JSON.PARSE(st3.Body)
    PRINT "   loaded dopo MODEL="; sj3("loaded")
  END IF

  PRINT "5) LLM SERVER STOP"
  LLM SERVER STOP
  PRINT "   server fermato. Fine."
END SUB
