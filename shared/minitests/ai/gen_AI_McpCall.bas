' EXPECT: ERROR: Server MCP 'x' non configurato (usa AI MCP SERVER ADD).
IMPORT AI

SUB Main()
    PRINT AI.MCPCALL("x", "x", "x")
END SUB
