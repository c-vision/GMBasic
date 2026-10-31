' EXPECT: 1|1|1
' MiniTest — AI.MCPRESOURCES(): un oggetto per ogni tool registrato (uri basic://tools/<nome>).
IMPORT AI

FUNCTION AddCents(amountA AS LONG, amountB AS LONG) AS STRING
    AddCents = STR$(amountA + amountB) + "c"
END FUNCTION

SUB Main()
    AI MCP REGISTER TOOL "zeta_tool", "AddCents", "Ultimo per nome"
    AI MCP REGISTER TOOL "alpha_tool", "AddCents", "Primo per nome", TRUE
    DIM r AS STRING
    r = AI.MCPRESOURCES()
    PRINT IIF(INSTR(r, "basic://tools/alpha_tool") > 0, 1, 0)
    PRINT IIF(INSTR(r, "basic://tools/zeta_tool") > 0, 1, 0)
    ' ordine deterministico (ordinato per nome): alpha prima di zeta
    PRINT IIF(INSTR(r, "basic://tools/alpha_tool") < INSTR(r, "basic://tools/zeta_tool"), 1, 0)
END SUB
