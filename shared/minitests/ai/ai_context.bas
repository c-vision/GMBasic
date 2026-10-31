' EXPECT: c1=context B: [A,B,C]|c0=context B: [B]|none=
IMPORT LLM
SUB A()
  B()
END SUB
SUB B()
  C()
END SUB
SUB C()
END SUB
SUB Main()
  A()
  PRINT "c1="; LLM.CONTEXT("B", 1)
  PRINT "c0="; LLM.CONTEXT("B", 0)
  PRINT "none="; LLM.CONTEXT("NonEsiste")
END SUB
