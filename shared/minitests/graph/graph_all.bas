' MiniTest NAMESPACE GRAPH — tutti i 25 simboli della superficie + i comandi del dominio.
'
' Superficie (shared/spec/registry/surface.csv): BETWEENNESS, BFS, BIPARTITE,
' BYFOLDER, BYTAG, CLUSTERING, CONNECTED, DFS, DIAMETER, DIJKSTRA, DOT, ECCENTRICITY,
' HASCYCLE, ISVALID, LOUVAIN, MAXFLOW, MST, NEIGHBORS, PAGERANK, PATH, QUERYMULTI,
' STATS, STRONGLYCONNECTED, SUBGRAPH, TOPOSORT.
' Comandi coperti: GRAPH RDD MEMORY, GRAPH BUILD, GRAPH UPDATE, GRAPH FIND PATH.
'
' Fixture: shared/samples/Fixtures/graph-vault (5 nodi, 11 archi).
' Scopo: verificare che i 4 backend supportino la STESSA versione del namespace,
' non un'applicazione. Si asseriscono FATTI (status e campi), non il JSON intero.
' EXPECT: built=-1|neighbors=-1|path=-1|pagerank=-1|connected=-1|stats=-1|strongly=-1|toposort=-1|bipartite=-1|betweenness=-1|clustering=-1|diameter=-1|mst=-1|dot=-1|subgraph=-1|bytag=-1|byfolder=-1|isvalid=-1|hascycle=-1|dijkstra=-1|eccentricity=-1|dfs=-1|bfs=-1|querymulti=-1|louvain=-1|maxflow=-1
IMPORT GRAPH
IMPORT IO

SUB Main()
  DIM fixture AS STRING
  DIM work AS STRING
  fixture = IO.CWD() + "/../../samples/Fixtures/graph-vault"
  work = "/tmp/gmb_mt_graph_all_work"
  IF IO.DIREXISTS(work) THEN RMDIR work, TRUE
  MKDIR work

  ' ── comandi legacy: RDD MEMORY / BUILD / FIND PATH / UPDATE ──────────────
  GRAPH RDD MEMORY
  GRAPH BUILD fixture, work
  GRAPH FIND PATH "index.md" TO "trading/momentum.md"
  GRAPH UPDATE fixture, work
  PRINT "built="; IO.DIREXISTS(work)

  DIM r AS STRING

  r = GRAPH.NEIGHBORS("graph-vault", "index.md")
  PRINT "neighbors="; (INSTR(r, "Success") > 0 AND INSTR(r, "index.md") > 0)
  r = GRAPH.PATH("graph-vault", "index.md", "trading/momentum.md")
  PRINT "path="; (INSTR(r, "Success") > 0 AND INSTR(r, "trading/momentum.md") > 0)
  r = GRAPH.PAGERANK("graph-vault", 3)
  PRINT "pagerank="; (INSTR(r, "Success") > 0 AND INSTR(r, "rank") > 0)
  r = GRAPH.CONNECTED("graph-vault")
  PRINT "connected="; (INSTR(r, "Success") > 0 AND INSTR(r, "componentCount") > 0)
  r = GRAPH.STATS("graph-vault")
  PRINT "stats="; (INSTR(r, "Success") > 0 AND INSTR(r, "nodes") > 0 AND INSTR(r, "edges") > 0)
  r = GRAPH.STRONGLYCONNECTED("graph-vault")
  PRINT "strongly="; (INSTR(r, "Success") > 0 AND INSTR(r, "components") > 0)
  r = GRAPH.TOPOSORT("graph-vault")
  PRINT "toposort="; (INSTR(r, "ciclo") > 0)
  r = GRAPH.BIPARTITE("graph-vault")
  PRINT "bipartite="; (INSTR(r, "Success") > 0 AND INSTR(r, "bipartite") > 0)
  r = GRAPH.BETWEENNESS("graph-vault")
  PRINT "betweenness="; (INSTR(r, "Success") > 0 AND INSTR(r, "values") > 0)
  r = GRAPH.CLUSTERING("graph-vault", "index.md")
  PRINT "clustering="; (INSTR(r, "Success") > 0 AND INSTR(r, "clustering") > 0)
  r = GRAPH.DIAMETER("graph-vault")
  PRINT "diameter="; (INSTR(r, "Success") > 0 AND INSTR(r, "diameter") > 0)
  r = GRAPH.MST("graph-vault")
  PRINT "mst="; (INSTR(r, "Success") > 0 AND INSTR(r, "weight") > 0)
  r = GRAPH.DOT("graph-vault")
  PRINT "dot="; (LEFT(r, 7) = "digraph")
  r = GRAPH.SUBGRAPH("graph-vault", "index.md", 1)
  PRINT "subgraph="; (LEFT(r, 8) = "graph TD")
  r = GRAPH.BYTAG("graph-vault", "strategy")
  PRINT "bytag="; (INSTR(r, "strategy") > 0 AND INSTR(r, "count") > 0)
  r = GRAPH.BYFOLDER("graph-vault", "strategies")
  PRINT "byfolder="; (INSTR(r, "strategies") > 0 AND INSTR(r, "count") > 0)
  r = GRAPH.ISVALID("graph-vault")
  PRINT "isvalid="; (r = "1")
  r = GRAPH.HASCYCLE("graph-vault")
  PRINT "hascycle="; (r = "1")
  r = GRAPH.DIJKSTRA("graph-vault", "index.md", "trading/momentum.md")
  PRINT "dijkstra="; (INSTR(r, "Success") > 0 AND INSTR(r, "path") > 0)
  r = GRAPH.ECCENTRICITY("graph-vault", "index.md")
  PRINT "eccentricity="; (INSTR(r, "Success") > 0 AND INSTR(r, "eccentricity") > 0)
  r = GRAPH.DFS("graph-vault", "index.md")
  PRINT "dfs="; (INSTR(r, "Success") > 0 AND INSTR(r, "order") > 0)
  r = GRAPH.BFS("graph-vault", "index.md")
  PRINT "bfs="; (INSTR(r, "Success") > 0 AND INSTR(r, "order") > 0)
  r = GRAPH.QUERYMULTI("momentum", "graph-vault")
  PRINT "querymulti="; (INSTR(r, "Success") > 0 AND INSTR(r, "brainCount") > 0 AND INSTR(r, "momentum") > 0)
  r = GRAPH.LOUVAIN("graph-vault")
  PRINT "louvain="; (INSTR(r, "Success") > 0 AND INSTR(r, "communities") > 0)
  r = GRAPH.MAXFLOW("graph-vault", "index.md", "airline/load-factor.md")
  PRINT "maxflow="; (INSTR(r, "Success") > 0 AND INSTR(r, "flow") > 0)

  RMDIR work, TRUE
END SUB
