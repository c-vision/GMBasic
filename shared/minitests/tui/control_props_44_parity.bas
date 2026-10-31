' Minitest TUI §4.4 props (GRID ALIGN/AUTOLITE, SPINNER INDEX, TEXTBOX FORMAT/VALID) — 4 backend.
' EXPECT: props44-ok
IMPORT CONTROLS
SCREEN 0
DIM v AS INTEGER
DIM s AS STRING

SUB Main()
  DIM idx AS INTEGER
  DIM al AS INTEGER
  DIM au AS INTEGER
  DIM vd AS INTEGER

  ' SPINNER: PRESET (nuova prop) + INDEX (letto).
  SPINNER 2, 2, "1|2|3" INTO v NAME "spn"
  CONTROL SET "spn", "preset", 0, "10|20|30"
  CONTROL SET "spn", "index", 0, ""
  CONTROL GET "spn", "index" INTO idx

  ' GRID: ALIGN + AUTOLITE.
  DIM hd(2) AS STRING
  DIM dt(1, 2) AS STRING
  hd(0) = "A" : hd(1) = "B"
  dt(0, 0) = "1" : dt(0, 1) = "2"
  GRID 5, 2, 40, 5, hd, dt INTO v NAME "grd"
  CONTROL SET "grd", "align", 2, ""
  CONTROL SET "grd", "autolite", 1, ""
  CONTROL GET "grd", "align" INTO al
  CONTROL GET "grd", "autolite" INTO au

  ' TEXTBOX (pixel): FORMAT + VALID.
  TEXTBOX 8, 2, 30, 1 INTO s NAME "txt"
  CONTROL SET "txt", "format", 0, "##/##"
  CONTROL SET "txt", "valid", 1, ""
  CONTROL GET "txt", "valid" INTO vd

  IF al = 2 AND au = 1 AND vd = 1 AND idx = 0 THEN PRINT "props44-ok" ELSE PRINT "props44-no " + STR(idx) + "|" + STR(al) + "|" + STR(au) + "|" + STR(vd)
END SUB
