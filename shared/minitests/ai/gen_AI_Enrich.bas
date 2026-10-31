' EXPECT: Error: nessun provider LLM configurato (AI SET ACTIVE)
IMPORT AI

SUB Main()
    PRINT AI.ENRICH("x", "x")
END SUB
