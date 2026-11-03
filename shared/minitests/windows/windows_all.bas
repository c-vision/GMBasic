' MiniTest NAMESPACE WINDOWS — tutte le funzioni della superficie.
' Derivato dai MiniTest per-funzione (stessi input e oracoli).
' EXPECT: w=-1  h=-1|x=-1|y=-1|theme_ok=-1|desktop=1|viewport=ok|wclosed= 0
IMPORT WINDOWS

SUB T0()
  PRINT "w="; WINDOWS.WIDTH(); " h="; WINDOWS.HEIGHT()
END SUB

SUB T1()
  IF WINDOWS.X() = -1 THEN PRINT "x=-1" ELSE PRINT "x=?"
  IF WINDOWS.Y() = -1 THEN PRINT "y=-1" ELSE PRINT "y=?"
  PRINT "theme_ok="; (LEN(WINDOWS.THEME()) >= 0)
  IF WINDOWS.DESKTOP() = 1 THEN PRINT "desktop=1" ELSE PRINT "desktop=?"
END SUB

SUB T2()
    VIEWPORT (1, 1)-(80, 25)
    VIEWPORT (2, 2)-(40, 10)
    VIEWPORT
    PRINT "viewport=ok"
END SUB

SUB Extra()
  PRINT "wclosed="; WINDOWS.WINDOWCLOSED()
END SUB

SUB Main()
  T0()
  T1()
  T2()
  Extra()
END SUB
