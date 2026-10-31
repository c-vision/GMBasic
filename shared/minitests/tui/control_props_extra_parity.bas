' Minitest CONTROL SET/GET props specifiche (max numericupdown, value; color label) — 4 backend.
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  NUMERICUPDOWN 5, 2, 12, 0, 100, 50 INTO v NAME "nud"
  SENDKEY CHR(27)
  CONTROL SET "nud", "max", 200, ""
  CONTROL SET "nud", "value", 80, ""
  CONTROL GET "nud", "max" INTO v
  HEXVIEW 6, 2, 30, 5, "abc" INTO v NAME "hex"
  SENDKEY CHR(27)
  CONTROL SET "hex", "bytesperrow", 16, ""
  CONTROL SET "hex", "showascii", 1, ""
  PRINT "OK-props-extra"
END SUB
