' MiniTest DATALIST — doppia sintassi (veloce posizionale | verbosa AT/SIZE/ITEMS).
' EXPECT: OK-datalist-dual
IMPORT CONTROLS

SUB Main()
  DIM d AS INTEGER
  DATALIST 1, 2, 20, 5, "a|b|c" INTO d              ' veloce
  DATALIST AT 7, 2 SIZE 20, 5 ITEMS "x|y|z" INTO d  ' verbosa
  PRINT "OK-datalist-dual"
END SUB
