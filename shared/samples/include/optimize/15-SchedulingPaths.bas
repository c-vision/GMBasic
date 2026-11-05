REM FILE: 15-SchedulingPaths.bas — pacchetto `scheduling` (FFI): cammini con finestre
REM temporali (OPTIMIZE.SCHEDPATHSTIMEWINDOWS). Scenario: manutenzione di un impianto in 5
REM attività con finestre [inizio, fine] e tempo minimo di connessione (pulizia/setup) fra
REM un'attività e la successiva. Si elencano i percorsi ammissibili e si sceglie il più corto.
REM Output deterministico, identico sui 4 backend.
IMPORT OPTIMIZE

SUB Main()
  ' Attività: 0 ispezione, 1 smontaggio, 2 riparazione, 3 collaudo, 4 riconsegna.
  ' Precedenze possibili (DAG): 0->1, 0->2, 1->3, 2->3, 1->2, 3->4.
  DIM adj(24) AS LONG
  adj(0*5+1)=1 : adj(0*5+2)=1
  adj(1*5+2)=1 : adj(1*5+3)=1
  adj(2*5+3)=1
  adj(3*5+4)=1

  ' Finestre: start {0,2,6,10,14}, end {1,4,9,13,15}, connessione minima 1.
  DIM win(10) AS DOUBLE
  win(0)=0 : win(1)=2 : win(2)=6 : win(3)=10 : win(4)=14
  win(5)=1 : win(6)=4 : win(7)=9 : win(8)=13 : win(9)=15
  win(10)=1

  ' Fino a 4 percorsi da 0 (ispezione) a 4 (riconsegna).
  DIM paths(19) AS LONG
  DIM lens(3) AS LONG
  n = OPTIMIZE.SCHEDPATHSTIMEWINDOWS(adj, win, 5, 0, 4, paths, lens, 4)
  PRINT "percorsi="; n;

  ' Stampa di ogni percorso (indici di nodo) e scelta del più corto.
  best = 99
  FOR k = 0 TO n - 1
    PRINT "|p"; k; "=";
    FOR j = 0 TO lens(k) - 1
      PRINT paths(k*5+j);
    NEXT j
    PRINT "("; lens(k); ")";
    IF lens(k) < best THEN best = lens(k)
  NEXT k
  PRINT "|piuCorto="; best

  ' Se il setup richiesto cresce, alcune connessioni non sono più ammissibili:
  ' con 3 non si passa da 0 a 1 (end0=1 + 3 > start1=2) e il grafo si spezza.
  win(10) = 3
  n2 = OPTIMIZE.SCHEDPATHSTIMEWINDOWS(adj, win, 5, 0, 4, paths, lens, 4)
  PRINT "|conSetup3="; n2

  ' Con un tempo di connessione impossibile non esiste alcun percorso.
  win(10) = 50
  n3 = OPTIMIZE.SCHEDPATHSTIMEWINDOWS(adj, win, 5, 0, 4, paths, lens, 4)
  PRINT "|impossibile="; n3
  PRINT ""
END SUB
