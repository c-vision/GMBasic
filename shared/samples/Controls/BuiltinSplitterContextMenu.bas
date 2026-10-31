' BuiltinSplitterContextMenu.bas — SPLITTER e CONTEXTMENU come controlli BUILT-IN (CONTROLS).
' Verbi di linguaggio (non UserControl): creano e registrano il controllo. Gira su 4 backend.
IMPORT CONTROLS
SCREEN 0

SUB Main()
  DIM sp AS INTEGER
  SPLITTER 2, 2, 40, 10, 0 INTO sp
  PRINT "splitter created"

  DIM cm AS INTEGER
  CONTEXTMENU 2, 13, 20, 6, ["Nuovo", "Apri", "--", "Esporta"] INTO cm
  PRINT "contextmenu created"

  PRINT "builtin-splitter-contextmenu-ok"
END SUB
