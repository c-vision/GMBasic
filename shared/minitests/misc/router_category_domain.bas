' MiniTest: orchestrazione (B5) — routing per categoria (attributo agente + CategoryRegistry).
' L'agente ha una categoria; il router la consulta prima del Differential Router.
' Deterministico: round-trip dell'attributo su tutti i backend.
' EXPECT: category=ultrabrain|router=OFF
IMPORT AI

SUB Main
  AI AGENT CREATE "myagent"
  AI AGENT SET "myagent" category ultrabrain
  PRINT "category="; AI.AGENTGET("myagent", "category")
  PRINT "router="; AI.AGENTGET("myagent", "router")
END SUB
