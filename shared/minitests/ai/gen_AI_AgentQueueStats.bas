' EXPECT: {"agent":"x","count":0,"paused":false,"stopped":false}
IMPORT AI

SUB Main()
    PRINT AI.AGENTQUEUESTATS("x")
END SUB
