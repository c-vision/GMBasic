' Minitest SPLITTER + CONTEXTMENU come controlli BUILT-IN (CONTROLS) — 4 backend.
' EXPECT: splitter-contextmenu-builtin-ok
IMPORT CONTROLS
SCREEN 0

SUB Main()
  DIM sp AS INTEGER
  SPLITTER 2, 2, 40, 10, 0 INTO sp
  DIM cm AS INTEGER
  CONTEXTMENU 2, 13, 20, 6, ["Apri", "Salva", "--", "Esporta"] INTO cm
  PRINT "splitter-contextmenu-builtin-ok"
END SUB
