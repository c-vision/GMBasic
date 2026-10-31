' MiniTest: zona TAB della PRINT a virgola conta i CODE-POINT Unicode, non i byte
' (canone 2026-09-30: la 'à' di 2 byte sfasava le zone di 1 su VM/C/C99Native).
' EXPECT: Età:           42
SUB Main()
  PRINT "Età:", 42
END SUB
