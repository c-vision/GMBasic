' Matrice WINDOW SHOW / WINDOW HIDE - parita' 4 backend.
' Copre: toggle ripetuti, ridondanze (SHOW/SHOW, HIDE/HIDE), alternanze. Headless i
' comandi sono no-op osservabili; l'output sono solo le righe PRINT (identiche su
' tutti i backend). L'interazione con SCREEN e' coperta dal sample dedicato.
IMPORT WINDOWS
SUB Main()
  PRINT "t0"
  WINDOW SHOW
  WINDOW HIDE
  WINDOW SHOW
  WINDOW SHOW
  WINDOW HIDE
  WINDOW HIDE
  PRINT "t1"
  WINDOW HIDE
  WINDOW SHOW
  WINDOW HIDE
  WINDOW SHOW
  PRINT "t2"
  PRINT "DONE"
END SUB
