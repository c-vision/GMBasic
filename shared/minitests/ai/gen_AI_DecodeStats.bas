' EXPECT: {"error":"'x' non ha ancora misure di decode"}
IMPORT AI
IMPORT LLM

SUB Main()
    PRINT LLM.DECODESTATS("x")
END SUB
