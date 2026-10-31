' MiniTest: SHUTDOWN / SETOPTION (comandi senza namespace, famiglia NET) su handle 0.
' Su handle non valido sono no-op graziosi (stesso contratto di NET.PEEK(0, N) in peek.bas).
' EXPECT: shut-ok|setopt-ok
IMPORT NET
SUB Main()
  SHUTDOWN 0, 1
  PRINT "shut-ok"
  SETOPTION 0, "TCP_NODELAY", 1
  PRINT "setopt-ok"
END SUB
