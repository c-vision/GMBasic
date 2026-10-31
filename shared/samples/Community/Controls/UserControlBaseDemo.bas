' UserControlBaseDemo.bas — base formale `Control` (namespace CONTROLS).
' Un controllo utente dichiara l'ereditarietà: `CLASS <nome> INHERITS Control`.
' (In precedenza l'accettazione era "rilassata"; ora la base è registrata e risolve.)
IMPORT CONTROLS

CLASS Panel INHERITS Control
  SUB Paint(g AS ANY)
    CONTROL DRAW g, 0                  ' clear
    CONTROL DRAW g, 1, 200             ' colore
    CONTROL DRAW g, 2, 0, 0, 30, 10    ' diagonale locale
  END SUB
END CLASS

SUB Main()
  DIM p AS Panel = NEW Panel()
  CONTROL ADD p AT 2, 2 SIZE 40, 12
  CONTROL REMOVE p
  PRINT "usercontrol-base-demo-ok"
END SUB
