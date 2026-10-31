' MiniTest: dominio TEAM (statement + funzioni di lettura AI.TEAM*) — parita' 4 backend.
' Solo operazioni DETERMINISTICHE (mai TEAM RUN, che richiede un completer LLM).
' Robustezza allo stato globale in-process (TeamServer/AgentServer singleton condivisi
' tra i run PCode/IL/VMC dello stesso processo): agenti e team creati in modo idempotente,
' e i membri REIMPOSTATI con REDEFINE all'inizio (sostituisce, non accumula) cosi' il
' risultato non dipende dallo stato lasciato dal run precedente.
'   AI AGENT CREATE AgentOne/Two/Three  (idempotente)
'   AI TEAM CREATE MiniTeamA/B          (idempotente)
'   REDEFINE = reset deterministico dei membri
' EXPECT: setup-ok|membersA=AgentOne,AgentTwo|membersB=AgentThree|after-remove=AgentOne|after-add=AgentOne,AgentTwo|recursionA= 3|recursionB= 1|latentA=embed|latentB=text|strategy-set|thoughts-set|coord-set|role-set|dag-set|complexity-set|reflection-set|critic-set|prune-set|message-sent|broadcast-sent|redefined=AgentThree,AgentFour
IMPORT AI

SUB Main
  AI AGENT CREATE AgentOne
  AI AGENT CREATE AgentTwo
  AI AGENT CREATE AgentThree
  AI TEAM CREATE MiniTeamA
  AI TEAM CREATE MiniTeamB
  PRINT "setup-ok"

  ' reset deterministico dei membri (indipendente dallo stato residuo)
  AI TEAM MiniTeamA REDEFINE "AgentOne,AgentTwo"
  AI TEAM MiniTeamB REDEFINE "AgentThree"
  PRINT "membersA="; AI.TEAMAGENTS("MiniTeamA")
  PRINT "membersB="; AI.TEAMAGENTS("MiniTeamB")

  ' REMOVE poi ri-ADD (ADD richiede l'agente esistente: creato sopra)
  AI TEAM MiniTeamA REMOVE AGENT AgentTwo
  PRINT "after-remove="; AI.TEAMAGENTS("MiniTeamA")
  AI TEAM MiniTeamA ADD AGENT AgentTwo
  PRINT "after-add="; AI.TEAMAGENTS("MiniTeamA")

  ' getter impostati nel run (deterministici)
  AI TEAM MiniTeamA SET RECURSION 3
  PRINT "recursionA="; AI.TEAMRECURSION("MiniTeamA")
  PRINT "recursionB="; AI.TEAMRECURSION("MiniTeamB")
  AI TEAM MiniTeamA SET LATENT embed
  PRINT "latentA="; AI.TEAMLATENT("MiniTeamA")
  PRINT "latentB="; AI.TEAMLATENT("MiniTeamB")

  ' statement di configurazione (marker)
  AI TEAM MiniTeamB SET STRATEGY sequential
  PRINT "strategy-set"
  AI TEAM MiniTeamA SET THOUGHTS 5
  PRINT "thoughts-set"
  AI TEAM MiniTeamA SET COORDINATOR AgentOne
  PRINT "coord-set"
  AI TEAM MiniTeamA SET ROLE AgentOne WORKER
  PRINT "role-set"
  AI TEAM MiniTeamB SET DAG 2, 3, 3
  PRINT "dag-set"
  AI TEAM MiniTeamB SET COMPLEXITY 0.7
  PRINT "complexity-set"
  AI TEAM MiniTeamB SET REFLECTION both
  PRINT "reflection-set"
  AI TEAM MiniTeamB SET CRITIC "spec|edge|logic", AgentThree
  PRINT "critic-set"
  AI TEAM MiniTeamB SET PRUNE 4
  PRINT "prune-set"

  ' messaggistica (accoda conversazione, nessun LLM)
  AI TEAM MESSAGE MiniTeamA, "hello from minitest"
  PRINT "message-sent"
  AI TEAM MiniTeamB BROADCAST "broadcast minitest"
  PRINT "broadcast-sent"

  ' redefine finale (sostituisce i membri con CSV)
  AI TEAM MiniTeamB REDEFINE "AgentThree,AgentFour"
  PRINT "redefined="; AI.TEAMAGENTS("MiniTeamB")
END SUB
