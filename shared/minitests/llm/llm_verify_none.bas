' EXPECT: v=verify: none|c=commit: none
' §4.10 SEMAPRAX (LLM): nessun patch nel programma -> VERIFY/COMMIT = none.
IMPORT LLM
SUB Main()
  PRINT "v="; LLM.VERIFY()
  PRINT "c="; LLM.COMMIT()
END SUB
