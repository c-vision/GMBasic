' Minitest TUI CANDLESTICK — parità 4 backend (candele OHLC: celle in TEXT, pixel in GRAPHICS).
' EXPECT: OK-candlestick
IMPORT CONTROLS
SUB Main()
  ' Dataset demo (CSV vuoto).
  CANDLESTICK 1, 2, 40, 10
  SENDKEY CHR(27)
  ' Candele esplicite: "o,h,l,c;o,h,l,c;...".
  CANDLESTICK 12, 2, 40, 10, "10,12,9,11;11,13,10,12;12,12.5,11,11.5;11.5,12,10,10.5"
  SENDKEY CHR(27)
  PRINT "OK-candlestick"
END SUB
