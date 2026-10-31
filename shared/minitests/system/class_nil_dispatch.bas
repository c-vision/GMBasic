' MiniTest: dispatch di metodo su riferimento NIL (`DIM c AS Cls` senza NEW).
' Deve essere un errore BASIC CATTURABILE su tutti e 5 i backend — prima: IL catturava,
' PCode/VM-C tacevano (nil), C99Native andava in SIGSEGV.
' Vedi Bugs/(open) - CLASS-MethodCall-Su-Nil-Dispatch-Divergente.md.
' EXPECT: caught|end
CLASS box
  PUBLIC DIM x AS INTEGER
  PUBLIC FUNCTION Get() AS INTEGER
    Get = x + 1
  END FUNCTION
END CLASS
SUB Main()
  ON ERROR GOTO H
  DIM c AS box
  PRINT c.Get()
  PRINT "no-err"
  GOTO D
H:
  PRINT "caught"
D:
  PRINT "end"
END SUB
