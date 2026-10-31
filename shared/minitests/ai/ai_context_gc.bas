' EXPECT: ctx a: segs=1 chars=1 budget=0 overflow=0|SYSTEM=0 HISTORY=1 TOOL=0 OTHER=0
' MiniTest: AI CONTEXT GC <agent> — eviction: segmenti duplicati (kind+text) (§3.6).
IMPORT AI
SUB Main()
  AI CONTEXT CREATE "a"
  AI CONTEXT ADD "a", "HISTORY", "x"
  AI CONTEXT ADD "a", "HISTORY", "x"
  AI CONTEXT GC "a"
  AI CONTEXT MAP "a"
END SUB
