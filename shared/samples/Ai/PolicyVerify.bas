' POLICY + VERIFY + TOOL.SEARCH + UI.FIND — CUA Fase 3+4
IMPORT AI WITH ALIAS
IMPORT TOOL WITH ALIAS
IMPORT UI WITH ALIAS

SUB Main()
  AI AGENT CREATE "tester"
  ' Policy: nega file delete, approva browser
  AI AGENT SET "tester" BROWSER %AUTO
  PRINT "browser="; AI.AGENTGET("tester", "browser")

  ' ToolRegistry dinamico (live dispatch runtime C)
  PRINT "TOOL.SEARCH browser="; TOOL.SEARCH("browser")
  PRINT "TOOL.SEARCH computer="; TOOL.SEARCH("computer")

  ' ActionResolver: UI.FIND via runtime C
  PRINT "UI.FIND Save="; UI.FIND("Save")

  AI AGENT DELETE "tester"
  PRINT "PolicyVerify completato."
END SUB
