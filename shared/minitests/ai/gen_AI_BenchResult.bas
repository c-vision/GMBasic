' EXPECT: {"error":"'x' non ha ancora un benchmark"}
IMPORT AI
IMPORT LLM

SUB Main()
    PRINT LLM.BENCHRESULT("x")
END SUB
