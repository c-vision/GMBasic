' TBROWSE con letterali ANNIDATI (forma vera di colonne/dati: {{intestazione, larghezza}, ...} e {{cella, ...}, ...}).
' Regressioni: IL segfault del CLR (string[] con double dentro), C99Native `'_array_' non definita`.
' EXPECT: OK-tbrowse-nested
IMPORT CONTROLS
SUB Main()
  DIM t AS STRING
  TBROWSE 1, 2, 30, 6, {{"Nome", 10}, {"Qta", 6}}, {{"aa", "1"}, {"bb", "2"}} INTO t
  PRINT "OK-tbrowse-nested"
END SUB
