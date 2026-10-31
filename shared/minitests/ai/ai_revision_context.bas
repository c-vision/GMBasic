' EXPECT: rev=rev=dfb23ef0339682c0|ctx=context B: [A,B,C,Main]|ok=match|bad=mismatch
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
  PRINT "rev="; LLM.REVISION()
  PRINT "ctx="; LLM.CONTEXT("B", 2)
  PRINT "ok="; LLM.CHECKREV("rev=dfb23ef0339682c0")
  PRINT "bad="; LLM.CHECKREV("rev=0000000000000000")
END SUB
