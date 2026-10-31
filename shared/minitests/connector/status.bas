' EXPECT: stlen= 470|nf={"error":"connector 'nonexistent' not found"}
IMPORT AI
SUB Main()
  PRINT "stlen="; LEN(AI.CONNECTORSTATUS("gmail"))
  PRINT "nf="; AI.CONNECTORSTATUS("nonexistent")
END SUB
