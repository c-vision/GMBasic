' MiniTest: AI.AskChain con agente inesistente -> errore esplicito (stringa, exit 0).
' Stesso pattern di gen_AI_AgentStatus/AgentGet ('Agent x is not registered').
' EXPECT: askchain=ERROR: Agent 'n' is not registered.
IMPORT AI WITH ALIAS
SUB Main()
  PRINT "askchain="; AI.AskChain("n", "p", "x")
END SUB
