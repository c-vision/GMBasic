' MiniTest UserControl base formale — `CLASS X INHERITS Control` risolve (namespace CONTROLS).
' EXPECT: ucbase-ok
IMPORT CONTROLS

CLASS Box INHERITS Control
  SUB Paint(g AS ANY)
  END SUB
END CLASS

SUB Main()
  DIM b AS Box = NEW Box()
  CONTROL ADD b AT 1, 1 SIZE 20, 8
  CONTROL REMOVE b
  PRINT "ucbase-ok"
END SUB
