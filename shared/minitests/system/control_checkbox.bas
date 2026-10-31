' EXPECT: control-ok
' Controlli TUI — CHECKBOX, LISTBOX, MENU, TOGGLE (parità 4 backend)
IMPORT CONTROLS
SUB Main()
  DIM r
  SENDKEY CHR(13)
  CHECKBOX 1, 1, "Test" INTO r
  SENDKEY CHR(13)
  LISTBOX 1, 2, 20, 5, "a,b,c" INTO r
  SENDKEY CHR(13) + CHR(13)
  MENU 1, 3, "a,b,c" INTO r
  SENDKEY CHR(13)
  TOGGLE 1, 4, "Test" INTO r
  PRINT "control-ok"
END SUB