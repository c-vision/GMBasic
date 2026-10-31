' FILE: minitests/nullable/nullable_isnil.bas
' `x IS NIL` su una variabile nullable.
' EXPECT: init-nil|val-notnil|nil-nil
SUB Main()
  DIM n AS INTEGER?
  IF n IS NIL THEN PRINT "init-nil"
  n = 5
  IF NOT (n IS NIL) THEN PRINT "val-notnil"
  n = NIL
  IF n IS NIL THEN PRINT "nil-nil"
END SUB
