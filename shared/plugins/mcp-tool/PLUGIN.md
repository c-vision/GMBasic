---
name: mcp-tool
description: Invokes a tool of an existing MCP server on behalf of the agent.
trigger: mcp tool, invoke tool, mcp server, call tool
version: 1.0.0
enabled: true
tools: [mcp_invoke]
requires: [AI, MCP]
procedures: [McpInvoke]
---
# Plugin mcp-tool

Operating instructions for the agent: use `AI.PLUGIN.CALL("mcp-tool", "server.tool args")` to invoke
a tool of an MCP server. The `McpInvoke` procedure uses `AI.MCPCALL`.
