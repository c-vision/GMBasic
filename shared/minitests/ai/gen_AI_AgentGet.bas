' EXPECT: ERROR: Agent 'x' is not registered.
IMPORT AI

SUB Main()
    PRINT AI.AGENTGET("x", "x")
END SUB
