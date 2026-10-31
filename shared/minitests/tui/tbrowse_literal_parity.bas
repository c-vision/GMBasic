' Minitest TUI TBROWSE con letterali array {..} come colonne/dati (prima: emitter C# `#error unsupported expression: ArrayLiteralExpr`).
IMPORT CONTROLS
SUB Main()
  DIM t AS STRING
  TBROWSE 1, 2, 20, 5, {"A", "B"}, {"1", "2"} INTO t
  PRINT "OK-tbrowse-literal"
END SUB
