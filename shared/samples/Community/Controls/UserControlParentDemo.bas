' UserControlParentDemo.bas — composizione ANNIDATA di UserControl (clausola PARENT).
' Un controllo utente può ospitarne un altro: le coordinate del figlio sono RELATIVE
' al contenuto del padre (compositing ricorsivo). Sintassi: `CONTROL ADD figlio ... PARENT padre`
' dove `padre` è la variabile-istanza del controllo contenitore.
IMPORT CONTROLS

CLASS Panel
  PRIVATE DIM mCol AS DOUBLE

  SUB SetColor(c AS DOUBLE)
    mCol = c
  END SUB

  SUB Paint(g AS ANY)
    CONTROL DRAW g, 0                  ' clear (sfondo del pannello)
    CONTROL DRAW g, 1, mCol            ' colore corrente
    CONTROL DRAW g, 2, 0, 0, 40, 40    ' diagonale locale (0,0)→(40,40)
  END SUB
END CLASS

SUB Main()
  DIM outer AS Panel = NEW Panel()
  DIM inner AS Panel = NEW Panel()
  outer.SetColor(65280)                ' verde
  inner.SetColor(16711680)             ' rosso

  ' Pannello esterno (root) e pannello interno ANNIDATO (coordinate relative a outer).
  CONTROL ADD outer AT 4, 2 SIZE 60, 20
  CONTROL ADD inner AT 2, 2 SIZE 30, 10 PARENT outer

  PRINT "usercontrol-parent-demo-ok"
END SUB
