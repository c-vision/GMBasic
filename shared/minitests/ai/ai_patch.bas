' EXPECT: p=patch target=B impact=[A@1,Main@2] decision=APPLY
IMPORT LLM
SUB A()
  B()
END SUB
SUB B()
END SUB
SUB Main()
  A()
  PRINT "p="; LLM.PATCH("B")
END SUB
