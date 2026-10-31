' EXPECT: ERROR: Agent 'x' is not registered.
' MiniTest: AI.AGENTRUNCODE <name>, <prompt>[, <rounds>] — loop agentico sul grafo del CODICE
' (§4.10, tool code.*). Parità 4 backend sul caso deterministico "agente non registrato":
' stesso testo esplicito del C# (nessun brain/fixture non deterministico nel gate).
IMPORT AI
SUB Main()
  PRINT AI.AGENTRUNCODE("x", "y", 1)
END SUB
