' Minitest props form-pixel (PROGRESS/TEXTBOX) via CONTROL SET/GET — parità 4 backend.
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  PROGRESS 5, 2, 20, 50 INTO v NAME "prog"
  CONTROL SET "prog", "max", 100, ""
  CONTROL SET "prog", "min", 0, ""
  CONTROL GET "prog", "max" INTO v
  PRINT "OK-form-props"
END SUB
