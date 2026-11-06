' WINDOWS.X/Y/THEME/DESKTOP — stato window manager (headless: -1/-1/"turbo"/1).
' Il nome del tema è quello CANONICO del registro C (gmb_ui_theme_name): minuscolo,
' default "turbo" — identico su IL/PCode/VM-C/C99Native.
' EXPECT: x=-1|y=-1|theme=turbo|desktop=1|set=nord
IMPORT WINDOWS
SUB Main()
  IF WINDOWS.X() = -1 THEN PRINT "x=-1" ELSE PRINT "x=?"
  IF WINDOWS.Y() = -1 THEN PRINT "y=-1" ELSE PRINT "y=?"
  IF WINDOWS.THEME() = "turbo" THEN PRINT "theme=turbo" ELSE PRINT "theme=?"
  IF WINDOWS.DESKTOP() = 1 THEN PRINT "desktop=1" ELSE PRINT "desktop=?"
  WINDOW THEME "nord"
  IF WINDOWS.THEME() = "nord" THEN PRINT "set=nord" ELSE PRINT "set=?"
  WINDOW THEME "turbo"
END SUB
