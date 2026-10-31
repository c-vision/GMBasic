' EXPECT: 0|80|25|0|16|-1|1|2|0|1
' SCREENINFO (DisplayMode: ModeID/Width/Height/Bpp/Colors/Active) e COLLIDEBATCH.
IMPORT GRAPHICS
DIM md, wd, ht, bp, cl, ac
md = SCREENINFO().ModeID
wd = SCREENINFO().Width
ht = SCREENINFO().Height
bp = SCREENINFO().Bpp
cl = SCREENINFO().Colors
ac = SCREENINFO().Active
DIM X#(1), Y#(1)
X#(0) = 10 : Y#(0) = 10
X#(1) = 12 : Y#(1) = 11
DIM CB() AS INTEGER
CB = COLLIDEBATCH(X, Y, 14, 14)
DIM ub, ln, c0, c1
ub = UBOUND(CB)
ln = LEN(CB)
c0 = CB(0)
c1 = CB(1)
PRINT md
PRINT wd
PRINT ht
PRINT bp
PRINT cl
PRINT ac
PRINT ub
PRINT ln
PRINT c0
PRINT c1
