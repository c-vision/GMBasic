' Macro su stringa COSTRUITA da parti costanti (concatenazione) e da una singola
' assegnazione costante: il Linker/parser le fondono a compile-time come il letterale
' (bug macro dinamiche con code block: `pre + "x| ... }"` non era espansa sui backend).
' EXPECT: c= 6|a= 6
SUB Main()
  DIM pre AS STRING = "{ |"
  DIM s AS STRING = pre + "x| RETURN x + 1 }"
  DIM b = &(s)
  PRINT "c="; EVAL(b, 5)
  DIM s2 AS STRING
  s2 = "{ |x| RETURN x + 1 }"
  DIM b2 = &(s2)
  PRINT "a="; EVAL(b2, 5)
END SUB
