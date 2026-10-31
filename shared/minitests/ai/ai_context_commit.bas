' EXPECT: view=SH
' MiniTest: AI CONTEXT COMMIT <agent> — view = segmenti (SYSTEM per primi, poi gli altri) (§3.6).
IMPORT AI
SUB Main()
  AI CONTEXT CREATE "a"
  AI CONTEXT ADD "a", "HISTORY", "H"
  AI CONTEXT ADD "a", "SYSTEM", "S"
  AI CONTEXT COMMIT "a"
  PRINT "view="; AI.CONTEXTVIEW$("a")
END SUB
