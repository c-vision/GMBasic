' AI.RULESADD / RULESFOR / RULESLIST / RULESREMOVE / RULESCLEAR
' Orchestrazione agenti (A5): Rules injection gerarchica — regole per directory, risolte per un
' path concatenando gli scope ancestrali (root → leaf), come AGENTS.md per-directory.
' Runtime condiviso: Basic.Agents.RuleRegistry (C#) / gmb_rules.c (C).
IMPORT AI

SUB Main
  DIM ok AS DOUBLE
  ok = AI.RULESADD("/progetto", "Regole globali del progetto")
  ok = AI.RULESADD("/progetto/src", "Regole per il codice sorgente")
  ok = AI.RULESADD("/progetto/src/compiler", "Regole del compilatore")
  PRINT "regole per /progetto/src/main.bas:"
  PRINT AI.RULESFOR("/progetto/src/main.bas")
  PRINT "scope registrati: "; AI.RULESLIST()
  ok = AI.RULESREMOVE("/progetto/src")
  ok = AI.RULESCLEAR()
END SUB
