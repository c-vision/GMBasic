' EXPECT: ok=verify: ok
' §4.10 SEMAPRAX (LLM): LLM.VERIFY() = receipt del gate revision-bound (rev corretta -> ok).
IMPORT LLM
SUB Greet()
  PRINT "orig"
END SUB
SUB Main()
  LLM PATCH "Greet", "rev=14650fb0739d0383"
    PRINT "patched"
  END PATCH
  PRINT "ok="; LLM.VERIFY()
END SUB
