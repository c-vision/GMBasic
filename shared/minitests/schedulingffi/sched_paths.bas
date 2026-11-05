' EXPECT: n= 3 : 4 , 3 , 3 : 0  1  2  3 : 0  1  3||nessuno= 0
' OPTIMIZE.SCHEDPATHSTIMEWINDOWS — cammini con finestre temporali (parità 4 backend).
' 4 attività in catena + scorciatoie: 0->1, 1->2, 2->3, 0->2, 1->3.
' Finestre: start {0,2,5,8}, end {1,3,6,9}, tempo minimo di connessione 1.
' Cammini 0->3 ammissibili: 0-1-2-3 (4 nodi), 0-1-3 (3), 0-2-3 (3).
IMPORT OPTIMIZE

SUB Main()
  DIM adj(15) AS LONG
  adj(0*4+1)=1
  adj(1*4+2)=1 : adj(1*4+3)=1
  adj(2*4+3)=1
  adj(0*4+2)=1

  DIM win(8) AS DOUBLE      ' start(4) | end(4) | minConnect
  win(0)=0 : win(1)=2 : win(2)=5 : win(3)=8
  win(4)=1 : win(5)=3 : win(6)=6 : win(7)=9
  win(8)=1

  DIM paths(11) AS LONG     ' maxPaths(3) * n(4)
  DIM lens(2) AS LONG
  r = OPTIMIZE.SCHEDPATHSTIMEWINDOWS(adj, win, 4, 0, 3, paths, lens, 3)
  PRINT "n="; r; ":"; lens(0); ","; lens(1); ","; lens(2); ":";
  PRINT paths(0); paths(1); paths(2); paths(3); ":";
  ' secondo cammino: 0-1-3
  PRINT paths(4); paths(5); paths(6)

  ' nessun cammino se la finestra non lascia connettere: minConnect enorme
  win(8) = 100
  r = OPTIMIZE.SCHEDPATHSTIMEWINDOWS(adj, win, 4, 0, 3, paths, lens, 3)
  PRINT "|nessuno="; r
  PRINT ""
END SUB
