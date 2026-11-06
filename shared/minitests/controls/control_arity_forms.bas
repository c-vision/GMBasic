' EXPECT: c3=ok c4=ok b3=ok b4=ok
' Forme POSIZIONALI dei controlli (parità 4 backend):
'   CHECKBOX x, y, caption      |  CHECKBOX x, y, w, caption
'   COMBOBOX x, y, w, items     |  COMBOBOX x, y, items
' La forma a 4 argomenti del CHECKBOX è quella usata dai sample grafici
' (shared/samples/Clipper/GraphicsControlsDemo.bas): prima il parser scartava il
' caption e mostrava la larghezza al suo posto.
IMPORT CONTROLS

SUB Main()
  DIM v AS INTEGER, s AS STRING
  DIM voci(1) AS STRING
  voci(0) = "IT" : voci(1) = "FR"

  CHECKBOX 2, 2, "notify" INTO v
  PRINT "c3=ok ";
  CHECKBOX 2, 4, 24, "notify" INTO v
  PRINT "c4=ok ";

  COMBOBOX 2, 6, 20, voci INTO s
  PRINT "b3=ok ";
  COMBOBOX 2, 8, voci INTO s
  PRINT "b4=ok"
END SUB
