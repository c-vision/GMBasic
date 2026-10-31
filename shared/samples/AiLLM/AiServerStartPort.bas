' PARITY-EXCLUDE: carica un modello LOCALE; solo manuale (sentinella).
'' FILE: Ai/AiServerStartPort.bas
' =================================================================================================
' AiServerStartPort — dimostra la PORTA OPZIONALE del comando LLM SERVER:
'   LLM SERVER START <modelDir> [port]     (default 8020)
' Avvia il server C su una porta scelta (8023), ne verifica lo stato via FETCH e lo ferma.
' Fail-loud: su errore (es. model dir inesistente) il comando alza un errore visibile.
' Richiede: GMBASIC_LLM_SERVER (o build nativa) + modello MLX (vedi AiServerDemo.bas).
' =================================================================================================
IMPORT AI WITH ALIAS
IMPORT LLM
IMPORT HTTP
IMPORT JSON

SUB Main
  DIM MODEL_DIR$ = "/Users/gaetano/ai/Models/Llama-3.2-1B-Instruct-4bit"
  IF NOT FileExists(MODEL_DIR$ + "/config.json") THEN
    PRINT "Modello non trovato: "; MODEL_DIR$
    PRINT "Modifica MODEL_DIR$ e riesegui."
    END
  END IF

  PRINT "1) LLM SERVER START "; MODEL_DIR$; " su porta 8023"
  LLM SERVER START MODEL_DIR$ 8023

  PRINT "2) GET /v1/status via FETCH (porta 8023)"
  DIM st AS FETCHRESULT = FETCH("http://127.0.0.1:8023/v1/status")
  IF st.IsSuccess THEN
    DIM sj AS JSON = JSON.PARSE(st.Body)
    PRINT "   loaded="; sj("loaded"); " model="; sj("model")
  ELSE
    PRINT "   status fallito: "; st.StatusCode
  END IF

  PRINT "3) LLM SERVER STOP"
  LLM SERVER STOP
  PRINT "   server fermato. Fine."
END SUB
