' EXPECT: [[Red, 255], [Green, 0], [Blue, 0], [Alpha, 255]]|[[ModeID, 0], [Width, 80], [Height, 25], [Bpp, 0], [Colors, 16], [Active, -1], [Description, Text 80x25]]|[]
IMPORT GRAPHICS
PRINT RGB(255, 0, 0, 255)
PRINT SCREENINFO()
DIM xs(0)
DIM ys(0)
PRINT COLLIDEBATCH(xs, ys, 10, 10)
