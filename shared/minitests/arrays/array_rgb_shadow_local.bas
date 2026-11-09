' MiniTest: locale `rgb` OMONIMO del builtin GRAPHICS.RGB.
' `rgb(k)` deve leggere l'elemento dell'array (DOUBLE), non il valore RgbaColor del builtin
' RGB(...): il parser marca la call col TokenKind GMB_TOK_Rgb solo perché il lessema è il
' keyword, e su C99Native i predicati di tipo (etype / PRINT) la tipavano TY_VAL, emettendo
' `gmb_val_to_display(rgb[...])` e `gmb_val_to_double(rgb[...])` su un `double*` → C non
' compilabile, mentre IL/PCode/VM-C leggevano l'elemento (parity JsonSave/GraphicsTour).
' EXPECT: uno= 1|due= 2|somma= 3
SUB Main()
  DIM rgb(2) AS DOUBLE
  rgb(0) = 1 : rgb(1) = 2
  PRINT "uno="; rgb(0)
  PRINT "due="; rgb(1)
  PRINT "somma="; rgb(0) + rgb(1)
END SUB
