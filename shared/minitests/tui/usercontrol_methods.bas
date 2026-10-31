' MiniTest Control metodi — zucchero `istanza.Move/SetFocus/ZOrder/Invalidate/Refresh(...)`
' su classi che ereditano Control (→ comando CONTROL ...), + comandi `CONTROL INVALIDATE/REFRESH`.
' EXPECT: ucmeth-ok
IMPORT CONTROLS

CLASS Box INHERITS Control
  SUB Paint(g AS ANY)
  END SUB
END CLASS

SUB Main()
  DIM b AS Box = NEW Box()
  CONTROL ADD b AT 1, 1 SIZE 20, 8

  ' zucchero metodo (istanza.Metodo)
  b.Move(5, 5)
  b.SetFocus()
  b.ZOrder(1)
  b.Invalidate()
  b.Refresh()

  ' forma comando equivalente
  CONTROL MOVE "b", 6, 6
  CONTROL FOCUS "b"
  CONTROL ZORDER "b", 1
  CONTROL INVALIDATE "b"
  CONTROL REFRESH "b"

  CONTROL REMOVE b
  PRINT "ucmeth-ok"
END SUB
