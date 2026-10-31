' EXPECT: verify: ok|commit: applied|patched
' §4.10 SEMAPRAX write-side lifecycle (namespace LLM): LLM PATCH (immediato, revision-bound)
' + LLM.VERIFY()/LLM.COMMIT() receipt (read-only). rev = call-graph Main->Greet.
IMPORT LLM
SUB Greet()
  PRINT "orig"
END SUB
SUB Main()
  LLM PATCH "Greet", "rev=baa4ff0afd9323a6"
    PRINT "patched"
  END PATCH
  PRINT LLM.VERIFY()
  PRINT LLM.COMMIT()
  Greet()
END SUB
