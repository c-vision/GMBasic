' MiniTest: orchestrazione (B4) — AI.AGENTSWARM con concorrenza BOUNDED.
' Ora il C costruisce system prompt/transcript/history come il C# (gap agent-runtime chiuso):
' l'output è byte-parity. Verifica anche l'ordine dei risultati per indice.
' EXPECT: swarm=[user: Ciao] (insufficient context)|[user: Ciao] (insufficient context)|[user: Ciao] (insufficient context)
IMPORT AI

SUB Main
  PRINT "swarm="; AI.AGENTSWARM(3, "sw", "Ciao", 2)
END SUB
