' AI.AGENTRUNCODE — l'agente interroga il grafo semantico del CODICE (§4.10, tool `code.*`).
' Il loop agente usa i tool code.* (symbol/callmap/impact/context/findsymbol/fileoutline/repomap)
' per rispondere a domande sulla STRUTTURA di un file BASIC (simboli, chiamate, impatto).
' Implementazioni INDIPENDENTI: C# `SemanticAgentLoop` (grafo C#) per IL/PCode; C
' `gmb_agent_run_code` + `gmb_semquery_file` (gmb_semgraph) per VM/C e C99Native — niente P/Invoke.
'
' NOTA: richiede un provider di completamento che faccia TOOL-CALLING (modello locale MLX/llama o
' provider remoto). Con la fixture-completion il loop non produce tool_calls e risponde senza
' interrogare il grafo — il sample COMPILA comunque e va eseguito con un modello reale.
IMPORT AI

SUB Main()
  ' file di codice da analizzare (relativo alla cartella del sample).
  DIM target AS STRING = PROJECTFOLDER() + "/../../Community/Ai/OnMessageSmoke.bas"

  AI AGENT CREATE "code-agent"

  ' Loop agente sul GRAFO DEL CODICE: il modello invoca i tool code.* e riassume.
  DIM answer AS STRING
  answer = AI.AGENTRUNCODE("code-agent", _
    "Elenca le procedure/funzioni definite nel file e chi chiama chi. " + _
    "Usa code.fileoutline per l'elenco e code.callmap per le relazioni, poi riassumi.", 5)
  PRINT "=== RISPOSTA ==="
  PRINT answer

  AI AGENT DELETE "code-agent"
  PRINT "done"
END SUB
