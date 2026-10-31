' PARITY-EXCLUDE: tool-loop C#-only: AgentToolLoop/FixtureToolCallingCompleter non eseguono sul C
' FILE: AI/AiAgentToolLoop.bas
' AiAgentToolLoop — round-trip agente con un tool di PLUGIN (web-fetch).
' Il completer fixture (FixtureToolCallingCompleter) emette la tool call web_fetch quando
' il prompt contiene il marker "tool:"; AgentToolLoop la invoca via IlMcpToolDispatcher →
' InvokeBasicProcedure("WebFetch") → la FUNCTION WebFetch del sample. End-to-end, senza rete.

IMPORT AI WITH ALIAS

FUNCTION WebFetch(url AS STRING) AS STRING
  WebFetch = "FETCHED:" + url
END FUNCTION

SUB Main()
  DIM answer$ AS STRING

  AI PLUGINS ENABLE "web-fetch"
  PRINT "Plugin web-fetch enabled"

  AI AGENT CREATE "WebAgent"
  AI AGENT REGISTER TOOL "WebAgent", "web_fetch"
  PRINT "Agent + tool registered"

  answer$ = AI.AGENTASKSYNC("WebAgent", "tool: web_fetch")
  PRINT "ANSWER="; answer$

  AI AGENT DELETE "WebAgent"
END SUB
