' EXPECT: ctx a: segs=1 chars=3 budget=4 overflow=0|SYSTEM=0 HISTORY=3 TOOL=0 OTHER=0
' MiniTest: AI CONTEXT TRIM <agent> — elimina i segmenti non-SYSTEM più vecchi fino al budget (§3.6).
IMPORT AI
SUB Main()
  AI CONTEXT SET BUDGET "a", 4
  AI CONTEXT CREATE "a"
  AI CONTEXT ADD "a", "HISTORY", "aaa"
  AI CONTEXT ADD "a", "HISTORY", "bbb"
  AI CONTEXT TRIM "a"
  AI CONTEXT MAP "a"
END SUB
