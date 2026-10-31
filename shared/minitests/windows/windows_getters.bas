' TAG: interactive
' WINDOWS.X/Y/THEME/DESKTOP — stato window manager (headless: -1/-1/"Turbo"/1).
' EXPECT: x=-1|y=-1|theme=Turbo|desktop=1
IMPORT WINDOWS
SUB Main()
  IF WINDOWS.X() = -1 THEN PRINT "x=-1" ELSE PRINT "x=?"
  IF WINDOWS.Y() = -1 THEN PRINT "y=-1" ELSE PRINT "y=?"
  IF WINDOWS.THEME() = "Turbo" THEN PRINT "theme=Turbo" ELSE PRINT "theme=?"
  IF WINDOWS.DESKTOP() = 1 THEN PRINT "desktop=1" ELSE PRINT "desktop=?"
END SUB
