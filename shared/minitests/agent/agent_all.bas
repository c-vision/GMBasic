' MiniTest NAMESPACE AGENT — unica funzione della superficie: RUN(goal, max_steps?).
' Contratto deterministico senza brain/modello: goal vuoto → messaggio esplicito
' "AGENT.RUN: goal mancante" (identico sulle 4 gambe). Il percorso con brain reale
' (gmb_agent_brain_register / GMB_BRAIN_URL) è coperto dal sample degli agenti.
' EXPECT: r=AGENT.RUN: goal mancante
IMPORT AGENT

SUB Main()
  PRINT "r="; AGENT.RUN("", 0)
END SUB
