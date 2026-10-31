' AI AGENT SET ... category + AI.AGENTGET — routing per categoria (B5, chiude A1).
' L'agente ha una categoria (deep/quick/ultrabrain/...); il router la consulta prima del
' Differential Router (AI.CATEGORYSET/CATEGORYAUTO). Runtime condiviso: AgentServer / gmb_agent.
IMPORT AI

SUB Main
  AI AGENT CREATE "planner"
  AI AGENT SET "planner" category ultrabrain
  AI AGENT SET "planner" router ON
  PRINT "categoria: "; AI.AGENTGET("planner", "category")
  PRINT "router: "; AI.AGENTGET("planner", "router")
END SUB
