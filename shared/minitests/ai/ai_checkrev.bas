' EXPECT: ok=match|bad=mismatch
IMPORT LLM
SUB A()
END SUB
SUB Main()
  A()
  PRINT "ok="; LLM.CHECKREV("rev=1026dd93addf8058")
  PRINT "bad="; LLM.CHECKREV("rev=0000000000000000")
END SUB
