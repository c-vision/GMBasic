' EXPECT: ctx a: segs=2 chars=2 budget=0 overflow=0|SYSTEM=1 HISTORY=1 TOOL=0 OTHER=0
' MiniTest: AI CONTEXT ADD <agent>, <kind>, <text> — appende un segmento (§3.6).
IMPORT AI
SUB Main()
  AI CONTEXT CREATE "a"
  AI CONTEXT ADD "a", "SYSTEM", "s"
  AI CONTEXT ADD "a", "HISTORY", "h"
  AI CONTEXT MAP "a"
END SUB
