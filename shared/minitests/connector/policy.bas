' EXPECT: default=allow|set=approve
IMPORT AI
SUB Main()
  AI CONNECTOR REGISTER "mt_pol", "test"
  PRINT "default="; AI.CONNECTORPOLICYGET("mt_pol", "send")
  AI CONNECTOR POLICY SET "mt_pol", "send", "approve"
  PRINT "set="; AI.CONNECTORPOLICYGET("mt_pol", "send")
  AI CONNECTOR UNREGISTER "mt_pol"
END SUB
