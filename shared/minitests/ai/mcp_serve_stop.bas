' EXPECT: ok|ok
' MiniTest — AI MCP SERVE con mode invalido (ritorna 0, NON blocca) e AI MCP STOP.
' Il serving reale (stdio/http) è verificato end-to-end da tools/test_mcp_e2e.sh.
IMPORT AI

SUB Main()
    AI MCP SERVE "bogus"
    PRINT "ok"
    AI MCP STOP
    PRINT "ok"
END SUB
