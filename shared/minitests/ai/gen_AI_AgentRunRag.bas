' EXPECT: ERROR: Agent 'x' is not registered.
IMPORT AI

SUB Main()
    PRINT AI.AGENTRUNRAG("x", "x", 1)
END SUB
