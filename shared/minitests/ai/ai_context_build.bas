' EXPECT: ctx a: segs=3 chars=27 budget=20 overflow=0|SYSTEM=3 HISTORY=24 TOOL=0 OTHER=0|ctx a: segs=2 chars=15 budget=20 overflow=0|SYSTEM=3 HISTORY=12 TOOL=0 OTHER=0|view=syshist_long_xx
' MiniTest: §3.6 PARSEC (lato AGENTE) — context builder a segmenti CREATE/ADD/MAP/TRIM/GC/COMMIT.
' Parità 4 backend (IL/PCodeVM/VM-C/C99Native).
IMPORT AI
SUB Main()
  AI CONTEXT SET BUDGET "a", 20
  AI CONTEXT CREATE "a"
  AI CONTEXT ADD "a", "SYSTEM", "sys"
  AI CONTEXT ADD "a", "HISTORY", "hist_long_xx"
  AI CONTEXT ADD "a", "HISTORY", "hist_long_xx"
  AI CONTEXT MAP "a"
  AI CONTEXT GC "a"
  AI CONTEXT TRIM "a"
  AI CONTEXT MAP "a"
  AI CONTEXT COMMIT "a"
  PRINT "view="; AI.CONTEXTVIEW$("a")
END SUB
