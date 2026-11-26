' MiniTest: widget TUI senza-namespace NON modali (10): COMBOBOX/GRID/SLIDER/RADIO/
' TREEVIEW/COLORPICKER/DATEPICKER/FILEDIALOG/TAB/PRINTERDIALOG.
' Arità/sintassi dal parser (Parser.Stmt.Controls.cs), NON dalla guida (che è imprecisa).
' CHIUSO 2026-11-26: non parcheggia e C99Native emette i 10 widget → `widgets-ok` su
' tutte e 4 le gambe (prima escluso dal cross-leg e marcato interactive).
' NOTA: TBROWSE/SPINNER/STATUSBAR esclusi (disegnano il framebuffer su stdout: — emitter C# emette il RENDER del framebuffer
' (gmb_tui_draw -> fwrite) mentre le gambe gestite renderizzano diversamente: divergenza
' di output in `widgets_draw_perleg.bas`, verificata per-gamba.
' EXPECT: widgets-ok
IMPORT CONTROLS
SUB Main()
  DIM s AS STRING
  DIM sd$ AS STRING
  DIM sf$ AS STRING
  DIM ps AS STRING
  DIM n
  DIM cp
  DIM dx
  COMBOBOX 1, 1, 10, "a,b" INTO s
  GRID 1, 2, 10, 5, "a,b", "1,2" INTO s
  SLIDER 1, 3, 10, 0, 100, 5 INTO n
  RADIO 1, 5, "a,b" INTO s
  TREEVIEW 1, 6, 10, 5, "a,b" INTO s
  COLORPICKER 1, 7, 4 INTO n
  DATEPICKER 1, 8 INTO sd$
  FILEDIALOG "*.txt", "apri" INTO sf$
  TAB 1, 10, 10, 5, "t1,t2" INTO s
  PRINTERDIALOG INTO ps, cp, dx
  PRINT "widgets-ok"
END SUB