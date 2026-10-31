' EXPECT: {"error":"'x' non risolve a un contesto KV con completer cache-manager (agente non registrato o fixture)"}
IMPORT AI
IMPORT LLM

SUB Main()
    PRINT LLM.CACHESTATS("x")
END SUB
