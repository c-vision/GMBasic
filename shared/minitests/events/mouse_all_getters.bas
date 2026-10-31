' EXPECT: btn-ok|dbl-ok|mid-ok|pres-ok|wheel-ok
IMPORT MOUSE
SUB Main()
  IF MOUSE.BUTTON() >= 0 THEN PRINT "btn-ok"
  IF MOUSE.DBLCLICK() >= 0 THEN PRINT "dbl-ok"
  IF MOUSE.MIDDLE() >= 0 THEN PRINT "mid-ok"
  IF MOUSE.PRESENT() >= 0 THEN PRINT "pres-ok"
  IF MOUSE.WHEEL() >= 0 THEN PRINT "wheel-ok"
END SUB