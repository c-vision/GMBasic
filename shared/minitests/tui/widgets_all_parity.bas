' Minitest TUI widgets — parità 4 backend: tutti i 18 controlli, chiusura SENDKEY ESC.
' Target: liste/dialoghi → STRING; numerici → LONG (arità canoniche Parser.Stmt.Controls.cs).
' EXPECT: OK-widgets_all
IMPORT CONTROLS
SUB Main()
  DIM n AS LONG
  DIM s AS STRING
  DIM ps AS STRING
  DIM cp AS LONG
  DIM dx AS LONG
  STATUSBAR "parity"
  SENDKEY CHR(27)
  TOGGLE 1, 1, "t" INTO n
  SENDKEY CHR(27)
  RADIO 2, 1, ["A", "B", "C"] INTO s
  SENDKEY CHR(27)
  SLIDER 3, 1, 40, 0, 100, 5 INTO n
  SENDKEY CHR(27)
  TAB 4, 1, 50, 3, ["A", "B", "C"] INTO n
  SENDKEY CHR(27)
  CHECKBOX 5, 1, "c" INTO n
  SENDKEY CHR(27)
  LISTBOX 6, 1, 30, 5, ["A", "B", "C"] INTO s
  SENDKEY CHR(27)
  COMBOBOX 7, 1, 20, ["IT", "FR", "DE"] INTO s
  SENDKEY CHR(27)
  MENU 8, 1, ["File", "Edit", "Help"] INTO s
  SENDKEY CHR(27)
  MENU 9, 1, [["File", "New", "Open"], ["Edit", "Cut"]] INTO s
  SENDKEY CHR(27)
  DATEPICKER 10, 1 INTO s
  SENDKEY CHR(27)
  COLORPICKER 11, 1, 16 INTO n
  SENDKEY CHR(27)
  GRID 12, 1, 60, 5, ["A", "B"], ["1", "2", "3", "4"] INTO n
  SENDKEY CHR(27)
  TREEVIEW 13, 1, 30, 10, ["root", "child"] INTO s
  SENDKEY CHR(27)
  TBROWSE 14, 1, 70, 15, ["A", "B"], ["1", "2", "3", "4"] INTO s
  SENDKEY CHR(27)
  FILEDIALOG "*.bas", "select" INTO s
  SENDKEY CHR(27)
  SPINNER 15, 1, "dots"
  SENDKEY CHR(27)
  PRINTERDIALOG INTO ps, cp, dx
  PRINT "OK-widgets_all"
END SUB
