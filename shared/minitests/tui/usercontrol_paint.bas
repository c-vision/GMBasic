' MiniTest UserControl (CONTROL ADD / REMOVE): classe con metodo Paint registrata come controllo.
' Verifica dominio end-to-end del verbo CONTROL ADD (parsing/AST/emissione) + parità 4 backend.
' EXPECT: OK-usercontrol
IMPORT CONTROLS

CLASS Sea
  SUB Paint(g AS ANY)
  END SUB
END CLASS

SUB Main()
  DIM c AS Sea = NEW Sea()
  CONTROL ADD c AT 10, 10 SIZE 100, 40
  CONTROL REMOVE c
  CONTROL ADD c, 10, 10, 100, 40
  CONTROL REMOVE c
  PRINT "OK-usercontrol"
END SUB
