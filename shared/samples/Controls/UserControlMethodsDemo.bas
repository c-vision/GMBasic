' UserControlMethodsDemo.bas — metodi Control (zucchero `istanza.Metodo(...)`).
' Su classi che ereditano Control: Move/SetFocus/ZOrder/Invalidate/Refresh.
IMPORT CONTROLS

CLASS Panel INHERITS Control
  SUB Paint(g AS ANY)
    CONTROL DRAW g, 0                  ' clear
    CONTROL DRAW g, 1, 65280           ' verde
    CONTROL DRAW g, 2, 0, 0, 30, 10    ' diagonale
  END SUB
END CLASS

SUB Main()
  DIM p AS Panel = NEW Panel()
  CONTROL ADD p AT 2, 2 SIZE 40, 12

  p.Move(5, 5)        ' ≡ CONTROL MOVE "p", 5, 5
  p.SetFocus()        ' ≡ CONTROL FOCUS "p"
  p.ZOrder(1)         ' ≡ CONTROL ZORDER "p", 1
  p.Invalidate()      ' marca da ridisegnare
  p.Refresh()         ' repaint

  CONTROL REMOVE p
  PRINT "usercontrol-methods-demo-ok"
END SUB
