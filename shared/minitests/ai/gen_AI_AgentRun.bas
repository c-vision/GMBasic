' EXPECT: ERROR: Agent 'x' is not registered.
IMPORT AI

SUB Main()
    PRINT AI.AGENTRUN("x", "x", 1)
END SUB
