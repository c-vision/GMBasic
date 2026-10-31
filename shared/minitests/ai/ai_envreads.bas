' MiniTest: AI.McpServerList / McpServerMarketplaceList / SkillsPrompt / MemoryRank.
' SENZA oracolo: l'output dipende dallo stato locale (config/mcp.json, skills/, brain.db)
' ma e' identico fra le gambe nello stesso ambiente -> la parita' cross-leg resta il test
' (precedente: gen_AI_BrainVaultQuery senza EXPECT). Copertura census per i 4 simboli.
IMPORT AI WITH ALIAS
SUB Main()
  PRINT "mcplist-len="; LEN(AI.McpServerList())
  PRINT "mcpmarket-len="; LEN(AI.McpServerMarketplaceList())
  PRINT "skillsp-len="; LEN(AI.SkillsPrompt())
  PRINT "memrank-len="; LEN(AI.MemoryRank("x", 1))
END SUB
