' EXPECT: 350|15|5|350|20|15
' VECTOR ADD WRAP (verbo su due parole): dest = wrap(dest + src, min, max).
IMPORT GRAPHICS
IMPORT VECTOR
DIM a#(2), b#(2)
a#(0) = 350 : a#(1) = 10 : a#(2) = -5
b#(0) = 0 : b#(1) = 5 : b#(2) = 10
VECTOR ADD WRAP a, b, 0, 360
PRINT a#(0)
PRINT a#(1)
PRINT a#(2)
VECTOR ADD WRAP a, b, 0, 360
PRINT a#(0)
PRINT a#(1)
PRINT a#(2)
