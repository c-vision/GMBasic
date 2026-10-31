' EXPECT: {"error":"'x' non ha ancora misure di prefill"}
IMPORT AI
IMPORT LLM

SUB Main()
    PRINT LLM.PREFILLSTATS("x")
END SUB
