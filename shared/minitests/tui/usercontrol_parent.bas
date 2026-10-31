' MiniTest UserControl composizione PARENT — annidamento controllo-in-controllo.
' `CONTROL ADD figlio AT x,y SIZE w,h PARENT padre` (padre = variabile-istanza, per nome).
' Verifica dominio end-to-end (parsing/AST/emissione) + parità 4 backend.
' EXPECT: ucparent-ok
IMPORT CONTROLS

CLASS Box
  SUB Paint(g AS ANY)
  END SUB
END CLASS

SUB Main()
  ' 2 livelli: contenitore = nome-variabile del padre.
  DIM outer AS Box = NEW Box()
  DIM inner AS Box = NEW Box()
  CONTROL ADD outer AT 2, 2 SIZE 60, 20
  CONTROL ADD inner AT 3, 3 SIZE 30, 10 PARENT outer
  CONTROL REMOVE inner
  CONTROL REMOVE outer

  ' 3 livelli (annidamento ricorsivo).
  DIM a AS Box = NEW Box()
  DIM b AS Box = NEW Box()
  DIM d AS Box = NEW Box()
  CONTROL ADD a AT 0, 0 SIZE 80, 30
  CONTROL ADD b AT 1, 1 SIZE 60, 20 PARENT a
  CONTROL ADD d AT 1, 1 SIZE 40, 10 PARENT b
  CONTROL REMOVE d
  CONTROL REMOVE b
  CONTROL REMOVE a

  PRINT "ucparent-ok"
END SUB
