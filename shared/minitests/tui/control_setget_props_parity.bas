' Minitest CONTROL SET/GET props numeriche (min/max/step) — parità 4 backend.
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  SLIDER 5, 2, 20, 10, 90, 40 INTO v NAME "sld"
  SENDKEY CHR(27)
  CONTROL SET "sld", "min", 10, ""
  CONTROL SET "sld", "max", 90, ""
  CONTROL SET "sld", "step", 5, ""
  CONTROL GET "sld", "max" INTO v
  PRINT "OK-control-props"
END SUB
