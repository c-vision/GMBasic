' EXPECT: ap=commit: applied
' §4.10 SEMAPRAX (LLM): LLM.COMMIT() = receipt del commit (patch verificato -> applied).
IMPORT LLM
SUB Greet()
  PRINT "orig"
END SUB
SUB Main()
  LLM PATCH "Greet", "rev=14650fb0739d0383"
    PRINT "patched"
  END PATCH
  PRINT "ap="; LLM.COMMIT()
END SUB
