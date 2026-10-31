' EXPECT: rlen= 768|empty=[]
IMPORT AI
SUB Main()
  PRINT "rlen="; LEN(AI.CONNECTORRESOURCES("gmail"))
  PRINT "empty="; AI.CONNECTORRESOURCES("nonexistent")
END SUB
