' FILE: AI/McpHttpSecure.bas
' McpHttpSecure — MCP tools over HTTP with API key auth (compile gate; SERVE blocks at runtime).

IMPORT AI WITH ALIAS

FUNCTION AddNumbers(a AS INTEGER, b AS INTEGER) AS INTEGER
  AddNumbers = a + b
END FUNCTION

SUB Main()
  DIM ok AS BIT
  DIM tools$ AS STRING

  AI MCP REGISTER TOOL "add_numbers", "AddNumbers", "Add two integers"
  PRINT "Registered:"; ok

  tools$ = AI.MCPLISTTOOLS()
  PRINT "Tools="; tools$

  ' Requires GMBASIC_MCP_API_KEY in environment.
  ' AI MCP SERVE "http"           ' default port 8765
  ' AI MCP SERVE "http", 8765     ' explicit port
END SUB
