' MiniTest: keyword usate come NOMI (variabili, DIM, parametri, FOR/NEXT, procedure,
' tipi, classi, campi) — parità 4 backend col parser C permissivo.
' Vedi Bugs/(solved) - Parser-STEP-Variabile-Accettata-C-Rifiutata-CSharp.md.
' EXPECT: vars= 10|dim= 5|fn= 12|until-ok|until-ok|for= 6|udt= 8|class= 7
TYPE loop
  for AS INTEGER
END TYPE
CLASS while
  PUBLIC FUNCTION Get() AS INTEGER
    Get = 7
  END FUNCTION
END CLASS
FUNCTION wend(x AS INTEGER) AS INTEGER
  wend = x * 2
END FUNCTION
SUB until()
  PRINT "until-ok"
END SUB
SUB Main()
  step = 1
  for = 2
  to = 3
  end = 4
  PRINT "vars="; step + for + to + end
  DIM next AS INTEGER
  next = 5
  PRINT "dim="; next
  PRINT "fn="; wend(6)
  CALL until()
  until()
  total = 0
  FOR step = 1 TO 3
    total = total + step
  NEXT step
  PRINT "for="; total
  DIM v AS loop
  v.for = 8
  PRINT "udt="; v.for
  DIM c = NEW while()
  PRINT "class="; c.Get()
END SUB
