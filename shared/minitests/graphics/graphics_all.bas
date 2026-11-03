' MiniTest NAMESPACE GRAPHICS — tutte le 15 funzioni della superficie.
' C2PX, C2PY, CELL, CELLSTR, COLLIDE, COLLIDEBATCH, P2CX, P2CY, Point, RGB,
' SCREEN, ScreenInfo, Screens, TextHeight, TextWidth. Headless (console 80x25).
' EXPECT: c2px= 6|c2py= 8|p2cx= 3|p2cy= 4|point=-1|cell= 7|cellstr=[]|screen= 0|collide= 0|tw= 25|th= 19|screens= 1|tw_linear=-1|th_stable=-1|rgb=[[Red, 255], [Green, 0], [Blue, 0], [Alpha, 255]]|modeid= 0|width= 80|height= 25|bpp= 0|colors= 16|active=-1|cb_ub= 1|cb0= 0|cb1= 1
IMPORT GRAPHICS

SUB TestCoords()
  PRINT "c2px="; C2PX(3, 2)
  PRINT "c2py="; C2PY(4, 2)
  PRINT "p2cx="; P2CX(6, 2)
  PRINT "p2cy="; P2CY(8, 2)
END SUB

SUB TestScalars()
  PRINT "point="; GRAPHICS.POINT(1, 2)
  PRINT "cell="; GRAPHICS.CELL(0, 0, 0)
  PRINT "cellstr=["; GRAPHICS.CELLSTR(0, 0); "]"
  PRINT "screen="; GRAPHICS.SCREEN()
  PRINT "collide="; GRAPHICS.COLLIDE(1, 2)
END SUB

SUB TestMetrics()
  PRINT "tw="; TextWidth("abc")
  PRINT "th="; TextHeight()
  PRINT "screens="; Screens()
  PRINT "tw_linear="; (TextWidth("MMM") >= TextWidth("M"))
  PRINT "th_stable="; (TextHeight() = TextHeight())
END SUB

SUB TestInfo()
  PRINT "rgb="; RGB(255, 0, 0, 255)
  PRINT "modeid="; SCREENINFO().ModeID
  PRINT "width="; SCREENINFO().Width
  PRINT "height="; SCREENINFO().Height
  PRINT "bpp="; SCREENINFO().Bpp
  PRINT "colors="; SCREENINFO().Colors
  PRINT "active="; SCREENINFO().Active

  DIM X#(1), Y#(1)
  X#(0) = 10 : Y#(0) = 10
  X#(1) = 12 : Y#(1) = 11
  DIM CB() AS INTEGER
  CB = COLLIDEBATCH(X, Y, 14, 14)
  PRINT "cb_ub="; UBOUND(CB)
  PRINT "cb0="; CB(0)
  PRINT "cb1="; CB(1)
END SUB

SUB Main()
  TestCoords()
  TestScalars()
  TestMetrics()
  TestInfo()
END SUB
