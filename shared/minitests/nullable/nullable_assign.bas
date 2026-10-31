' FILE: minitests/nullable/nullable_assign.bas
' Assegnazione valore ↔ NIL su un nullable (con inizializzatore nella DIM).
' EXPECT: assign= 42|released|init= 100|init-has
SUB Main()
  DIM n AS INTEGER?
  n = 42
  PRINT "assign="; n.VALUE
  n = NIL
  IF n IS NIL THEN PRINT "released"
  DIM x AS INTEGER? = 100
  PRINT "init="; x.VALUE
  IF x HAS VALUE THEN PRINT "init-has"
END SUB
