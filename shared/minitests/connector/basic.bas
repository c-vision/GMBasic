' EXPECT: status={"error":"connector 'nonexistent' not found"}|tools=[]|search=[]|policy=allow|secret=|resources=[]|prompts=[]
IMPORT AI
SUB Main()
  PRINT "status="; AI.CONNECTORSTATUS("nonexistent")
  PRINT "tools="; AI.CONNECTORTOOLS("nonexistent")
  PRINT "search="; AI.CONNECTORSEARCH("nonexistent")
  PRINT "policy="; AI.CONNECTORPOLICYGET("x", "y")
  PRINT "secret="; AI.CONNECTORSECRETGET("x", "y")
  PRINT "resources="; AI.CONNECTORRESOURCES("x")
  PRINT "prompts="; AI.CONNECTORPROMPTS("x")
END SUB
