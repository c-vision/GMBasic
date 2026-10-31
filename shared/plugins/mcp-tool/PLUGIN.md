---
name: mcp-tool
description: Invoca un tool di un server MCP esistente per conto dell'agente.
trigger: tool mcp, invocare tool, server mcp, chiamare tool
version: 1.0.0
enabled: true
tools: [mcp_invoke]
requires: [AI, MCP]
procedures: [McpInvoke]
---
# Plugin mcp-tool

Istruzioni operative per l'agente: usa `AI.PLUGIN.CALL("mcp-tool", "server.tool args")`
per invocare un tool di un server MCP. La procedura `McpInvoke` usa `AI.MCPCALL`.
