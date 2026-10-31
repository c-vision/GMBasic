' EXPECT: 1|1|1|1|1
' MiniTest — AI MCP REGISTER TOOL (firma completa: tool, proc [, desc [, readOnly [, outputSchema]]])
' + AI.MCPLISTTOOLS() (inputSchema dai parametri, outputSchema, readOnly) + AI MCP UNREGISTER TOOL.
' Oracolo deterministico via INSTR: nessuna dipendenza da provider/rete.
IMPORT AI

FUNCTION AddCents(amountA AS LONG, amountB AS LONG) AS STRING
    AddCents = STR$(amountA + amountB) + "c"
END FUNCTION

SUB Main()
    AI MCP REGISTER TOOL "add_cents", "AddCents", "Somma in centesimi", TRUE, "{""type"":""string""}"
    DIM t AS STRING
    t = AI.MCPLISTTOOLS()
    PRINT IIF(INSTR(t, "add_cents") > 0, 1, 0)
    PRINT IIF(INSTR(t, "readOnly") > 0, 1, 0)
    PRINT IIF(INSTR(t, "outputSchema") > 0, 1, 0)
    PRINT IIF(INSTR(t, "amountA") > 0, 1, 0)
    AI MCP UNREGISTER TOOL "add_cents"
    DIM t2 AS STRING
    t2 = AI.MCPLISTTOOLS()
    PRINT IIF(INSTR(t2, "add_cents") = 0, 1, 0)
END SUB
