' EXPECT: ERROR: Agent 'x' is not registered.
IMPORT AI

SUB Main()
    PRINT AI.AGENTSTATUS("x")
END SUB
