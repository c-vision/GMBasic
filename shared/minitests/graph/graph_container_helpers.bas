' GRAPH (FFI) — helper di contenitore: FINDPATH, EDGELIST, NEIGHBORS.
' Grafo non orientato a 5 nodi:
'   0-1 (1), 1-2 (1), 2-3 (1), 0-4 (10), 4-3 (10)
' Il cammino minimo 0→3 è unico: 0-1-2-3 (costo 3, contro 20 via 4).
' EXPECT: nb= 2|: 1 , 4||dij= 1|:3.0||path= 4|: 0 - 1 - 2 - 3||edges= 5|: 0 - 1 :1.0|: 3 - 4 :10.0||bad=-1|:-1||f=0
IMPORT GRAPH

SUB Main()
  DIM n AS LONG
  n = 5
  DIM adj(24) AS LONG
  DIM w(24) AS DOUBLE
  DIM i AS LONG, j AS LONG
  FOR i = 0 TO 24
    w(i) = GRAPH.ABSENT()
  NEXT i
  ' archi non orientati (matrice simmetrica)
  adj(0*5+1)=1 : adj(1*5+0)=1
  adj(1*5+2)=1 : adj(2*5+1)=1
  adj(2*5+3)=1 : adj(3*5+2)=1
  adj(0*5+4)=1 : adj(4*5+0)=1
  adj(4*5+3)=1 : adj(3*5+4)=1
  w(0*5+1)=1  : w(1*5+0)=1
  w(1*5+2)=1  : w(2*5+1)=1
  w(2*5+3)=1  : w(3*5+2)=1
  w(0*5+4)=10 : w(4*5+0)=10
  w(4*5+3)=10 : w(3*5+4)=10

  ' ── NEIGHBORS: riga 0 → 1 e 4 ─────────────────────────────────────────────
  DIM nb(4) AS LONG
  PRINT "nb="; GRAPH.NEIGHBORLIST(adj, n, 0, nb)
  PRINT ":"; nb(0); ","; nb(1)

  ' ── DIJKSTRA + FINDPATH: 0→3 = 0,1,2,3 ────────────────────────────────────
  DIM dist(4) AS DOUBLE, prev(4) AS LONG
  PRINT "|dij="; GRAPH.DIJKSTRA(w, n, 0, dist, prev)
  PRINT ":"; FORMAT(dist(3), "#.0")
  DIM path(4) AS LONG
  PRINT "|path="; GRAPH.FINDPATH(prev, n, 0, 3, path)
  PRINT ":"; path(0); "-"; path(1); "-"; path(2); "-"; path(3)

  ' ── EDGELIST: 5 archi, in ordine di riga/colonna ──────────────────────────
  DIM eu(9) AS LONG, ev(9) AS LONG, ew(9) AS DOUBLE
  PRINT "|edges="; GRAPH.EDGELIST(w, n, eu, ev, ew)
  PRINT ":"; eu(0); "-"; ev(0); ":"; FORMAT(ew(0), "#.0")
  PRINT ":"; eu(4); "-"; ev(4); ":"; FORMAT(ew(4), "#.0")

  ' ── errore: nodo fuori range / cap troppo piccolo ─────────────────────────
  PRINT "|bad="; GRAPH.NEIGHBORLIST(adj, n, 9, nb)
  DIM eu2(1) AS LONG, ev2(1) AS LONG, ew2(1) AS DOUBLE
  PRINT ":"; GRAPH.EDGELIST(w, n, eu2, ev2, ew2)
  PRINT "|f=0"
END SUB
