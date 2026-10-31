' EXPECT: 1|1|1
' MiniTest — AI MCP REGISTER AGENT (tool "agent-<nome>") + AI.MCPPROMPTS() (agenti come prompt).
IMPORT AI

SUB Main()
    AI MCP REGISTER AGENT "analista"
    DIM t AS STRING
    t = AI.MCPLISTTOOLS()
    PRINT IIF(INSTR(t, "agent-analista") > 0, 1, 0)

    AI AGENT CREATE "helper"
    DIM p AS STRING
    p = AI.MCPPROMPTS()
    PRINT IIF(INSTR(p, "helper") > 0, 1, 0)
    PRINT IIF(INSTR(p, "Chat with agent helper") > 0, 1, 0)
END SUB
