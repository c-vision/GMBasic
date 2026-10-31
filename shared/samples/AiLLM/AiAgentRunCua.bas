' PARITY-EXCLUDE: browser/CDP: client CDP solo C# per architettura
' AI.AGENTRUN — loop Brain→Azione NATIVO C# con il modello locale GMBasic (embedding MLX).
' Il modello decide le azioni browser.* (reali via CDP); le osservazioni rientrano nella
' sessione KV fino alla risposta finale.
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
  ' 1) modello locale MLX come provider di completamento attivo (GMBasic embedding)
  AI REGISTER "llm-local", "completion", "mlx", "{""modelDir"":""/Users/gaetano/ai/Models/Qwen3.8-27B-8bit""}"
  AI SET ACTIVE "completion", "llm-local"
  PRINT "chat attivo="; AI.ACTIVE(%CHAT)

  ' 2) agente
  AI AGENT CREATE "agent1"
  AI AGENT SET "agent1" BROWSER %AUTO

  ' 3) loop Brain→Azione nativo (sessione; niente native-dispatch)
  DIM answer$ AS STRING
  answer$ = AI.AGENTRUN("agent1", "Determina il titolo della pagina https://example.com usando gli strumenti: naviga e poi leggi document.title.", 4)
  PRINT "=== RISPOSTA ==="
  PRINT answer$

  AI AGENT DELETE "agent1"
  PRINT "done"
END SUB
