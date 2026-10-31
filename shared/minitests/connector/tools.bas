' EXPECT: tlen= 1649|empty=[]
IMPORT AI
SUB Main()
  PRINT "tlen="; LEN(AI.CONNECTORTOOLS("gmail"))
  PRINT "empty="; AI.CONNECTORTOOLS("nonexistent")
END SUB
