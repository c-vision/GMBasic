' TOOL.SEARCH — CUA ToolRegistry dinamico (evita prompt da 500 tool)
IMPORT TOOL WITH ALIAS
IMPORT UI WITH ALIAS

SUB Main()
  DIM result$ AS STRING
  result$ = TOOL.SEARCH("pdf")
  PRINT "TOOL.SEARCH pdf="; result$

  result$ = TOOL.SEARCH("browser")
  PRINT "TOOL.SEARCH browser="; result$

  result$ = TOOL.SEARCH("computer")
  PRINT "TOOL.SEARCH computer="; result$

  ' UI semantico (ActionResolver API→AX→DOM→CUA) — stub per ora
  DIM found$ AS STRING
  found$ = UI.FIND("Save")
  PRINT "UI.FIND Save="; found$
  IF UI.CLICK("Save") THEN PRINT "UI.CLICK Save OK"
END SUB
