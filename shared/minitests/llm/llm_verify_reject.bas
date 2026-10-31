' EXPECT: v=verify: rejected|c=commit: rejected|orig
' §4.10 SEMAPRAX (LLM): patch a revisione ERRATA -> VERIFY/COMMIT rejected, corpo originale.
IMPORT LLM
SUB Greet()
  PRINT "orig"
END SUB
SUB Main()
  LLM PATCH "Greet", "rev=0000000000000000"
    PRINT "patched"
  END PATCH
  PRINT "v="; LLM.VERIFY()
  PRINT "c="; LLM.COMMIT()
  Greet()
END SUB
