' WINDOWS.X/Y/THEME/DESKTOP — stato window manager (headless: -1/-1/"turbovision"/1).
' Il nome del tema è quello CANONICO del registro C (gmb_ui_theme_name): minuscolo,
' default "turbovision" (TV 2.0) — identico su IL/PCode/VM-C/C99Native.
' EXPECT: x=-1|y=-1|theme=turbovision|desktop=1|set=nord
IMPORT WINDOWS
SUB Main()
  IF WINDOWS.X() = -1 THEN PRINT "x=-1" ELSE PRINT "x=?"
  IF WINDOWS.Y() = -1 THEN PRINT "y=-1" ELSE PRINT "y=?"
  IF WINDOWS.THEME() = "turbovision" THEN PRINT "theme=turbovision" ELSE PRINT "theme=?"
  IF WINDOWS.DESKTOP() = 1 THEN PRINT "desktop=1" ELSE PRINT "desktop=?"
  WINDOW THEME "nord"
  IF WINDOWS.THEME() = "nord" THEN PRINT "set=nord" ELSE PRINT "set=?"
  WINDOW THEME "turbo"
END SUB
