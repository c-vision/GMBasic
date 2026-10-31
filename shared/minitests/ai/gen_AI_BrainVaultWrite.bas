' EXPECT: Error: nessun provider LLM configurato (AI SET ACTIVE)
IMPORT AI

SUB Main()
    PRINT AI.BRAINVAULTWRITE("x", 1)
END SUB
