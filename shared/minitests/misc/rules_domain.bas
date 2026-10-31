' MiniTest: orchestrazione (A5) — Rules injection gerarchica (AGENTS.md per-directory).
' Deterministico: RULESFOR(path) concatena gli scope ancestrali (root → leaf).
' Runtime condiviso Basic.Agents.RuleRegistry (C#) / gmb_rules.c (C).
' EXPECT: add= 1|for-src=root|src|for-root=root|list={"rules":[{"scope":"/proj","content":"root"},{"scope":"/proj/src","content":"src"},{"scope":"/proj/src/lib","content":"lib"}]}|remove= 1|clear= 2
IMPORT AI

SUB Main
  DIM ok AS DOUBLE
  ok = AI.RULESADD("/proj", "root")
  ok = AI.RULESADD("/proj/src", "src")
  ok = AI.RULESADD("/proj/src/lib", "lib")
  PRINT "add="; ok
  PRINT "for-src="; AI.RULESFOR("/proj/src/file.bas")
  PRINT "for-root="; AI.RULESFOR("/proj/README.md")
  PRINT "list="; AI.RULESLIST()
  ok = AI.RULESREMOVE("/proj/src")
  PRINT "remove="; ok
  PRINT "clear="; AI.RULESCLEAR()
END SUB
