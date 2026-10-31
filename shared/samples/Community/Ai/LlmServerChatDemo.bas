' PARITY-EXCLUDE: server LLM esterno (gmb_llm_server/OpenAIServer su :8020): presente/assente e modello/risposte variano per macchina (2026-09-23, Parita-5-Backend-Residui §2B)
' FILE: Ai/LlmServerChatDemo.bas
' =================================================================================================
' LlmServerChatDemo — usa il server LLM OpenAI-compatible LOCALE di GMBasic da BASIC.
'
' RICHIEDE un server attivo su http://127.0.0.1:8020 (due versioni indipendenti):
'   - C  (core nativo): native/gmb_llm_server/build/gmb_llm_server ~/ai/Models/<dir> 8020
'   - C# (core managed): OpenAIServer  --model ~/ai/Models/<dir> --port 8020
' Se il server non e' raggiungibile il sample stampa le istruzioni ed esce (exit pulita).
'
' Endpoint dimostrati: GET /v1/status, GET /v1/models, POST /v1/chat/completions,
' POST /v1/completions (con `stop` stringa), POST /v1/models/unload + /load (residenza).
' Vedi guida: AiWiki Guide/utente/LLM-Server-OpenAI-Compatibile.md.
' NB: i letterali stringa NON iniziano/finiscono con `"` (tre virgolette consecutive =
' raw string): ogni body JSON e' un unico letterale per riga, che inizia con `{` e finisce
' con `}` (le virgolette interne sono raddoppiate `""`, come negli altri sample).
' =================================================================================================
IMPORT HTTP
IMPORT JSON

SUB Main
  DIM baseUrl$ = "http://127.0.0.1:8020"

  ' --- 1. GET /v1/status: server raggiungibile? ---
  DIM st AS FETCHRESULT = FETCH(baseUrl$ + "/v1/status")
  IF NOT st.IsSuccess THEN
    PRINT "Server non raggiungibile su "; baseUrl$
    PRINT "Avvialo prima (vedi intestazione) e riesegui:"
    PRINT "  native/gmb_llm_server/build/gmb_llm_server ~/ai/Models/Llama-3.2-1B-Instruct-4bit 8020"
    END
  END IF
  DIM statusJson AS JSON = JSON.PARSE(st.Body)
  PRINT "Stato   : "; statusJson("status"); "  (loaded: "; statusJson("loaded"); ")"

  ' --- 2. GET /v1/models: modello residente ---
  DIM ml AS FETCHRESULT = FETCH(baseUrl$ + "/v1/models")
  DIM modelsJson AS JSON = JSON.PARSE(ml.Body)
  PRINT "Modello : "; modelsJson("data")(0)("id")
  PRINT

  ' --- 3. POST /v1/chat/completions (non-stream) ---
  PRINT "--- /v1/chat/completions (non-stream) ---"
  DIM co AS HTTPOPTIONS = NEW HTTPOPTIONS
  co.Method = "POST"
  co.Headers.ContentType = "application/json"
  co.Timeout = 120000
  co.Body = "{""model"":""Llama-3.2-1B-Instruct-4bit"",""max_tokens"":32,""temperature"":1,""messages"":[{""role"":""system"",""content"":""You are a helpful assistant.""},{""role"":""user"",""content"":""Name the capital of France.""}]}"
  DIM chat AS FETCHRESULT = FETCH(baseUrl$ + "/v1/chat/completions", co)
  IF chat.IsSuccess THEN
    DIM chatJson AS JSON = JSON.PARSE(chat.Body)
    PRINT "Risposta: "; chatJson("choices")(0)("message")("content")
    PRINT "  finish_reason = "; chatJson("choices")(0)("finish_reason")
  ELSE
    PRINT "Errore chat: "; chat.StatusCode
  END IF
  PRINT

  ' --- 4. POST /v1/completions con `stop` (stringa) ---
  PRINT "--- /v1/completions (con stop) ---"
  co.Body = "{""prompt"":""The best programming language is"",""max_tokens"":64,""temperature"":1,""stop"":""!""}"
  DIM comp AS FETCHRESULT = FETCH(baseUrl$ + "/v1/completions", co)
  IF comp.IsSuccess THEN
    DIM compJson AS JSON = JSON.PARSE(comp.Body)
    PRINT "Testo  : "; compJson("choices")(0)("text")
    PRINT "  finish_reason = "; compJson("choices")(0)("finish_reason")
  ELSE
    PRINT "Errore completions: "; comp.StatusCode
  END IF
END SUB
