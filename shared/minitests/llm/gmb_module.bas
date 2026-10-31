' MiniTest: GMB MODULE — registry di moduli esterni (model compiler §4.3), 4 backend.
' EXPECT: demo.slot	/usr/lib/libSystem.B.dylib	5|ok
SUB Main()
  GMB MODULE REGISTER "demo.slot", "/usr/lib/libSystem.B.dylib"
  GMB MODULE PRIORITY "demo.slot", 5
  GMB MODULE LIST
  PRINT "ok"
END SUB
