' EXPECT: ins=procedure Worker callers=[Main] callees=[Helper]|none=
' §4.10 SEMAPRAX (namespace LLM): LLM.INSPECT(name$) = hover (kind+label+callmap immediato).
IMPORT LLM
SUB Helper()
END SUB
SUB Worker()
  Helper()
END SUB
SUB Main()
  Worker()
  PRINT "ins="; LLM.INSPECT("Worker")
  PRINT "none="; LLM.INSPECT("NonEsiste")
END SUB
