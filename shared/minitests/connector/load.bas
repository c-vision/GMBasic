' EXPECT: slack= 124
IMPORT AI
SUB Main()
  AI CONNECTOR LOAD "slack"
  PRINT "slack="; INSTR(AI.CONNECTORSTATUS("slack"), "true")
END SUB
