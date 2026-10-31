' Minitest TUI LISTBOX + colori di stato Clipper (COLOR/COLORFOCUS/COLORSEL/COLORERROR) — 4 backend.
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  LISTBOX 5, 2, 30, 5, ["uno", "due", "tre"] INTO v COLOR "W+/B" COLORFOCUS "GR+/B" COLORSEL "N/W" COLORERROR "W+/R"
  SENDKEY CHR(27)
  PRINT "OK-listbox-color-states"
END SUB
