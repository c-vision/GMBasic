' EXPECT: ERROR: Agent 'x' is not registered.
IMPORT AI

SUB Main()
    PRINT AI.AGENTASKSYNC("x", "x")
END SUB
