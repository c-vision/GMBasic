' FILE: AI/AgentBasic.bas
' AgentBasic — AI AGENT CREATE + AI.AGENTASK with fixture completion provider (no network).

IMPORT AI WITH ALIAS

SUB Main()
  DIM answer$ AS STRING
  DIM model$ AS STRING

  AI AGENT CREATE Support
  AI AGENT SET "Support" "systemPrompt" "You are a concise assistant."
  PRINT "Agent created"

  answer$ = AI.AGENTASKSYNC("Support", "What is two plus two?")
  PRINT "ASK="; answer$

  model$ = AI.AGENTGET("Support", "model")
  PRINT "Model="; model$

  AI AGENT SAVE "Support", "support.agent.json"
  PRINT "Saved"

  AI AGENT DELETE "Support"
  PRINT "Deleted"

  AI AGENT LOAD "Support", "support.agent.json"
  PRINT "Loaded"

  answer$ = AI.AGENTASKSYNC("Support", "Repeat the question.")
  PRINT "ASK after load="; answer$

  AI AGENT DELETE "Support"
END SUB

ON ERROR RESUME NEXT
KILL "support.agent.json"
