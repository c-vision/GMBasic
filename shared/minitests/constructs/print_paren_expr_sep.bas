' MiniTest: parentesi di ESPRESSIONE nel PRINT seguite da separatore.
' `PRINT (1); 2` non deve consumare '(' come wrapper della lista e lasciare
' ';' come inizio di una nuova istruzione.
' EXPECT: a 5 b|c 1 d
FUNCTION Main() AS INTEGER
  PRINT "a"; (2 + 3); "b"
  PRINT "c"; (1); "d"
  RETURN 0
END FUNCTION
