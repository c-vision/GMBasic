' EXPECT: stlen= 264|haskind= 34|after={"error":"connector 'mt_reg' not found"}
IMPORT AI
SUB Main()
  AI CONNECTOR REGISTER "mt_reg", "github"
  DIM s AS STRING = AI.CONNECTORSTATUS("mt_reg")
  PRINT "stlen="; LEN(s)
  PRINT "haskind="; INSTR(s, "github")
  AI CONNECTOR UNREGISTER "mt_reg"
  PRINT "after="; AI.CONNECTORSTATUS("mt_reg")
END SUB
