' MiniTest: AI.List con ruolo fixture -> lista deterministica (nessun provider).
' EXPECT: list-n= 1|agent0=fixture-completion
IMPORT AI WITH ALIAS
SUB Main()
  DIM l = AI.List("agent")
  PRINT "list-n="; LEN(l)
  PRINT "agent0="; l(0)
END SUB
