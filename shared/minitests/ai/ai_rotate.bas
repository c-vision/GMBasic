' MiniTest: AI.Rotate con ruolo fixture -> risposta deterministica (nessun provider).
' EXPECT: rotate=fixture-completion
IMPORT AI WITH ALIAS
SUB Main()
  PRINT "rotate="; AI.Rotate("agent")
END SUB
