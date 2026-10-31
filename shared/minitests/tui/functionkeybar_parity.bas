' Minitest TUI FUNCTIONKEYBAR — parità 4 backend (verbo CONTROLS, riga tasti funzione).
IMPORT CONTROLS
SUB Main()
  FUNCTIONKEYBAR 1, 1, 70, "1Help|2Save|3Load|4Find|5Run|6Edit|7List|8Auto|9Del|10Quit"
  SENDKEY CHR(27)
  FUNCTIONKEYBAR 2, 1, 40
  SENDKEY CHR(27)
  PRINT "OK-functionkeybar"
END SUB
