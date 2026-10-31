' FILE: AI/McpServerBasic.bas
' McpServerBasic — register a FUNCTION as MCP tool (compile gate; SERVE blocks only at runtime).

IMPORT AI WITH ALIAS

FUNCTION AddNumbers(a AS INTEGER, b AS INTEGER) AS INTEGER
  AddNumbers = a + b
END FUNCTION

SUB Main()
  DIM ok AS BIT
  DIM tools$ AS STRING

  AI MCP REGISTER TOOL "add_numbers", "AddNumbers", "Add two integers"
  PRINT "Registered"; ok

  tools$ = AI.MCPLISTTOOLS()
  PRINT "Tools="; tools$

  ' AI MCP SERVE "stdio"   ' Uncomment when launched as MCP subprocess (blocks on stdin)
END SUB
