' Minitest CONTROL SET/GET proprietà (incluso colore Clipper) — parità 4 backend.
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  LABEL 5, 2, 20, "demo" NAME "lbl"
  CONTROL SET "lbl", "color", 0, "W+/B"
  CONTROL SET "lbl", "focuscolor", 0, "GR+/B"
  CONTROL SET "lbl", "uppercase", 1, ""
  CONTROL GET "lbl", "width" INTO v
  PRINT "OK-control-setget"
END SUB
