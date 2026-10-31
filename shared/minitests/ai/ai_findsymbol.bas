' EXPECT: f=find Unit: [UnitScale]
IMPORT LLM
SUB A()
  UnitScale()
END SUB
SUB UnitScale()
END SUB
SUB Main()
  A()
  PRINT "f="; LLM.FINDSYMBOL("Unit")
END SUB
