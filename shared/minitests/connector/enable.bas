' EXPECT: disabled= 121|enabled= 121
IMPORT AI
SUB Main()
  AI CONNECTOR REGISTER "mt_en", "test"
  AI CONNECTOR DISABLE "mt_en"
  PRINT "disabled="; INSTR(AI.CONNECTORSTATUS("mt_en"), "false")
  AI CONNECTOR ENABLE "mt_en"
  PRINT "enabled="; INSTR(AI.CONNECTORSTATUS("mt_en"), "true")
  AI CONNECTOR UNREGISTER "mt_en"
END SUB
