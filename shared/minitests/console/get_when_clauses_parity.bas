' EXPECT: OK-get-when
' Minitest @...GET con clausole Clipper WHEN/VALID/MESSAGE — parità 4 backend.
IMPORT CONSOLE
SUB Main()
  DIM x AS INTEGER
  @ 5, 2 GET x WHEN x < 10 VALID x <= 100 MESSAGE "1..100" ERROR "fuori range"
  PRINT "OK-get-when"
END SUB
