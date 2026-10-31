' EXPECT: ctx a: segs=0 chars=0 budget=0 overflow=0|SYSTEM=0 HISTORY=0 TOOL=0 OTHER=0
' MiniTest: AI CONTEXT CREATE <agent> — azzera i segmenti dello store del context builder (§3.6).
IMPORT AI
SUB Main()
  AI CONTEXT ADD "a", "SYSTEM", "x"
  AI CONTEXT CREATE "a"
  AI CONTEXT MAP "a"
END SUB
