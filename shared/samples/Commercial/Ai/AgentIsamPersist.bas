// FILE: AI/AgentIsamPersist.bas
' AgentIsamPersist — AI AGENT SAVE / AI AGENT LOAD with transactional ISAM store (agents.isdb).

IMPORT AI WITH ALIAS

SUB Main()
  DIM ok AS BIT
  DIM answer$ AS STRING
  DIM prompt$ AS STRING

  AI AGENT CREATE BookingAgent
  AI AGENT SET "BookingAgent" "systemPrompt" "You remember booking context."
  prompt$ = "Reserve table for two at 8pm."
  answer$ = AI.AGENTASKSYNC("BookingAgent", prompt$)
  PRINT "ASK="; answer$

  AI AGENT SAVE BookingAgent
  PRINT "Saved to ISAM."

  AI AGENT DELETE "BookingAgent"
  PRINT "Deleted from memory:"; ok

  AI AGENT LOAD BookingAgent
  PRINT "Loaded from ISAM."

  answer$ = AI.AGENTASKSYNC("BookingAgent", "What was my last request?")
  PRINT "ASK after LOAD="; answer$

  AI AGENT DELETE BookingAgent
END SUB
