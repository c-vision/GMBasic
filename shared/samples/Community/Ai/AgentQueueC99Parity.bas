' PARITY-EXCLUDE: nondeterministico: ID GUID + timestamp ISO vs epoch
' FILE: Ai/AgentQueueC99Parity.bas — fase C: AI.AGENTSEND/RECEIVE/PAUSE/CHANNEL su C99
' (emissione gmb_agent_queue_*/gmb_bus_*). Puramente C, nessun LLM. Esito: stampa OK.
IMPORT AI WITH ALIAS

SUB Main()
    AI AGENT SEND W1, "TaskA", "inputA"
    AI AGENT SEND W1, "TaskB", "inputB"
    DIM m$ AS STRING
    AI AGENT RECEIVE W1 INTO m$
    PRINT "RECV="; m$
    AI AGENT PAUSE W1
    AI AGENT RESUME W1
    AI CHANNEL CREATE Chan1
    AI CHANNEL PUBLISH Chan1, "hello"
    PRINT "OK"
END SUB
