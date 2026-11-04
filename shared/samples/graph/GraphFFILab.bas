REM FILE: GraphFFILab.bas — pacchetto `graph`/`flow` (FFI): TUTTE le 43 funzioni.
REM Un unico grafo da 4 nodi (0-1-2 triangolo + 3 appeso), pesato e non, e una rete di flusso
REM 0->3. Ogni riga stampa un valore di controllo deterministico (stesso sui 4 backend).
REM Il namespace `GRAPH` è la TEORIA DEI GRAFI (matrici); il grafo dei cervelli è `BRAINS`.
IMPORT GRAPH

SUB Main()
  DIM adj(15) AS LONG
  DIM w(15) AS DOUBLE
  INF = GRAPH.ABSENT()
  ' archi non pesati: 0-1, 1-2, 2-0, 2-3
  adj(0*4+1)=1 : adj(1*4+0)=1
  adj(1*4+2)=1 : adj(2*4+1)=1
  adj(2*4+0)=1 : adj(0*4+2)=1
  adj(2*4+3)=1 : adj(3*4+2)=1
  ' stessi archi pesati (1, 2, 3, 1)
  DIM i AS LONG, j AS LONG
  FOR i = 0 TO 3
    FOR j = 0 TO 3
      w(i*4+j) = INF
    NEXT j
    w(i*4+i) = 0
  NEXT i
  w(0*4+1)=1 : w(1*4+0)=1
  w(1*4+2)=2 : w(2*4+1)=2
  w(2*4+0)=3 : w(0*4+2)=3
  w(2*4+3)=1 : w(3*4+2)=1

  ' ── 1. Struttura: grado, cicli, bipartizione, visite, componenti ─────────────────
  DIM ord(3) AS LONG
  DIM comp(3) AS LONG
  DIM o15(15) AS LONG
  PRINT "degree="; GRAPH.DEGREE(adj, 4, 0);
  PRINT "|cycle="; GRAPH.HASCYCLE(adj, 4);
  PRINT "|bip="; GRAPH.ISBIPARTITE(adj, 4);
  PRINT "|bfs="; GRAPH.BFS(adj, 4, 0, ord); ":"; ord(0);
  PRINT "|dfs="; GRAPH.DFS(adj, 4, 0, ord); ":"; ord(0);
  PRINT "|topo="; GRAPH.TOPOSORT(adj, 4, ord);
  PRINT "|comp="; GRAPH.COMPONENTS(adj, 4, comp);
  PRINT "|scc="; GRAPH.SCC(adj, 4, comp)

  ' ── 2. Cammini e distanze ────────────────────────────────────────────────────────
  DIM dist(3) AS DOUBLE
  DIM prev(3) AS LONG
  PRINT "dij="; GRAPH.DIJKSTRA(w, 4, 0, dist, prev); ":"; FORMAT(dist(3), "#.0");
  PRINT "|bf="; GRAPH.BELLMANFORD(w, 4, 0, dist, prev); ":"; FORMAT(dist(3), "#.0");
  DIM fw(15) AS DOUBLE
  PRINT "|fw="; GRAPH.FLOYDWARSHALL(w, 4, fw); ":"; FORMAT(fw(0*4+3), "#.0");
  DIM hh(3) AS DOUBLE
  DIM path(3) AS LONG
  DIM info(1) AS LONG
  PRINT "|astar="; FORMAT(GRAPH.ASTAR(w, 4, hh, 0, 3, path, info), "#.0");
  DIM kp(15) AS LONG
  DIM kl(1) AS LONG
  PRINT "|kshort="; GRAPH.KSHORTEST(w, 4, 0, 3, 2, kp, kl); ":"; kl(0)
  PRINT "|bfsopt="; GRAPH.BFSOPT(adj, 4, 0, 1.0, dist)

  ' ── 3. Centralità e comunità ─────────────────────────────────────────────────────
  DIM rank(3) AS DOUBLE
  DIM bc(3) AS DOUBLE
  DIM cc(3) AS DOUBLE
  DIM comm(3) AS LONG
  PRINT "pr="; GRAPH.PAGERANK(w, 4, rank, 0.85, 50); ":"; FORMAT(rank(0), "#.000");
  PRINT "|btw="; FORMAT(GRAPH.BETWEENNESS(w, 4, bc), "#.000");
  PRINT "|close="; GRAPH.CLOSENESS(adj, 4, cc); ":"; FORMAT(cc(0), "#.000");
  PRINT "|louvain="; GRAPH.LOUVAIN(w, 4, comm, 1.0)

  ' ── 4. Strutture: colorazione, clique, chiusura, alberi, ponti ───────────────────
  DIM color(3) AS LONG
  DIM clique(3) AS LONG
  DIM tc(15) AS LONG
  PRINT "col="; GRAPH.COLORING(adj, 4, color);
  PRINT "|clique="; GRAPH.MAXCLIQUE(adj, 4, clique);
  PRINT "|tc="; GRAPH.TRANSITIVECLOSURE(adj, 4, tc);
  PRINT "|diam="; FORMAT(GRAPH.DIAMETER(w, 4), "#.0");
  PRINT "|ecc="; FORMAT(GRAPH.ECCENTRICITY(w, 4, 0), "#.0");
  PRINT "|clu="; FORMAT(GRAPH.CLUSTERING(adj, 4, 0), "#.000");
  DIM nodes(1) AS LONG
  DIM sg(3) AS DOUBLE
  nodes(0)=0 : nodes(1)=1
  PRINT "|sub="; GRAPH.SUBGRAPH(w, 4, nodes, 2, sg);
  DIM eu(2) AS LONG
  DIM ev(2) AS LONG
  DIM ew(2) AS DOUBLE
  PRINT "|mst="; GRAPH.MSTKRUSKAL(w, 4, eu, ev, ew);
  DIM isb(15) AS LONG
  PRINT "|bridges="; GRAPH.BRIDGES(adj, 4, isb);
  DIM art(3) AS LONG
  PRINT "|artic="; GRAPH.ARTICULATION(adj, 4, art)
  PRINT "|stoer="; FORMAT(GRAPH.STOERWAGNER(w, 4), "#.0");
  DIM mapping(3) AS LONG
  PRINT "|iso="; GRAPH.ISOMORPHIC(adj, 4, adj, 4, mapping);
  DIM lab(3) AS LONG
  PRINT "|drnl="; GRAPH.DRNL(adj, 4, 0, 3, lab);
  DIM colors(3) AS LONG
  PRINT "|wl="; GRAPH.WL(adj, 4, colors);
  PRINT "|mincover="; GRAPH.MINPATHCOVER(adj, 4)

  ' ── 5. Flussi e matching (flow.inc) ──────────────────────────────────────────────
  DIM cap(15) AS DOUBLE
  FOR i = 0 TO 15 : cap(i) = 0 : NEXT i
  cap(0*4+1)=3 : cap(1*4+3)=2 : cap(0*4+2)=2 : cap(2*4+3)=3
  PRINT "maxflow="; FORMAT(GRAPH.MAXFLOW(cap, 4, 0, 3), "#.0");
  DIM side(3) AS LONG
  PRINT "|mincut="; FORMAT(GRAPH.MINCUT(cap, 4, 0, 3, side), "#.0");
  PRINT "|dinic="; FORMAT(GRAPH.DINIC(cap, 4, 0, 3), "#.0");
  DIM cost(15) AS DOUBLE
  FOR i = 0 TO 15 : cost(i) = 1 : NEXT i
  DIM fv(0) AS DOUBLE
  PRINT "|mincost="; FORMAT(GRAPH.MINCOSTFLOW(cap, cost, 4, 0, 3, fv), "#.0");
  DIM bm(1) AS LONG
  DIM bmr(1) AS LONG
  DIM bip(3) AS LONG
  bip(0*2+0)=1 : bip(0*2+1)=1 : bip(1*2+0)=1
  PRINT "|bipmatch="; GRAPH.BIPARTITEMATCH(bip, 2, 2, bm, bmr);
  DIM gm(3) AS LONG
  PRINT "|genmatch="; GRAPH.GENERALMATCH(adj, 4, gm);
  DIM wm(3) AS LONG
  ' matching pesato: serve una matrice COMPLETA e finita (con ABSENT() non esiste matching)
  DIM wc(15) AS DOUBLE
  FOR i = 0 TO 3
    FOR j = 0 TO 3
      IF i = j THEN
        wc(i*4+j) = 0
      ELSE
        wc(i*4+j) = 1
      END IF
    NEXT j
  NEXT i
  PRINT "|wmatch="; FORMAT(GRAPH.WEIGHTEDMATCH(wc, 4, wm), "#.0");
  DIM lo(15) AS DOUBLE
  DIM up(15) AS DOUBLE
  DIM bb(3) AS DOUBLE
  DIM fl(15) AS DOUBLE
  FOR i = 0 TO 15 : lo(i) = 0 : up(i) = cap(i) : fl(i) = 0 : NEXT i
  FOR i = 0 TO 3 : bb(i) = 0 : NEXT i
  PRINT "|circ="; GRAPH.CIRCULATION(lo, up, 4, bb, fl);
  DIM gh_p(3) AS LONG
  DIM gh_w(3) AS DOUBLE
  PRINT "|gomoryhu="; GRAPH.GOMORYHU(w, 4, gh_p, gh_w);
  DIM commd(15) AS DOUBLE
  DIM cflow(15) AS DOUBLE
  FOR i = 0 TO 15 : commd(i) = 0 : cflow(i) = 0 : NEXT i
  commd(0)=1 : commd(1)=3 : commd(2)=1
  PRINT "|multicomm="; FORMAT(GRAPH.MULTICOMMODITY(cap, 4, 1, commd, cflow), "#.0")
  PRINT ""
END SUB
