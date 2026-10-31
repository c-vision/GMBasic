' FILE: minitests/nullable/nullable_hasvalue.bas
' `x HAS VALUE` su una variabile nullable: vero con valore, falso con NIL.
' EXPECT: init-none|val-has|nil-none
SUB Main()
  DIM n AS INTEGER?
  IF n HAS VALUE THEN PRINT "init-has" ELSE PRINT "init-none"
  n = 42
  IF n HAS VALUE THEN PRINT "val-has"
  n = NIL
  IF NOT (n HAS VALUE) THEN PRINT "nil-none"
END SUB
