' MiniTest NAMESPACE BRAINS — 25 simboli (inclusi BYTAG/BYFOLDER/QUERYMULTI) + i comandi BRAINS del dominio.
'
' Superficie (shared/spec/registry/surface.csv): BETWEENNESS, BFS, BIPARTITE,
' BYFOLDER, BYTAG, CLUSTERING, CONNECTED, DFS, DIAMETER, DIJKSTRA, DOT, ECCENTRICITY,
' HASCYCLE, ISVALID, LOUVAIN, MAXFLOW, MST, NEIGHBORS, PAGERANK, PATH, QUERYMULTI,
' STATS, STRONGLYCONNECTED, SUBGRAPH, TOPOSORT.
' Comandi coperti: BRAINS RDD MEMORY, BRAINS BUILD, BRAINS UPDATE, BRAINS FIND PATH.
'
' Fixture: shared/samples/Fixtures/graph-vault (5 nodi, 11 archi).
' Scopo: verificare che i 4 backend supportino la STESSA versione del namespace,
' non un'applicazione. Si asseriscono FATTI (status e campi), non il JSON intero.
' EXPECT: built=-1|neighbors=-1|path=-1|pagerank=-1|connected=-1|stats=-1|strongly=-1|toposort=-1|bipartite=-1|betweenness=-1|clustering=-1|diameter=-1|mst=-1|dot=-1|subgraph=-1|bytag=-1|byfolder=-1|isvalid=-1|hascycle=-1|dijkstra=-1|eccentricity=-1|dfs=-1|bfs=-1|querymulti=-1|louvain=-1|maxflow=-1
IMPORT BRAINS
IMPORT IO

SUB Main()
  DIM fixture AS STRING
  DIM work AS STRING
  fixture = IO.CWD() + "/../../samples/Fixtures/graph-vault"
  work = "/tmp/gmb_mt_graph_all_work"
  IF IO.DIREXISTS(work) THEN RMDIR work, TRUE
  MKDIR work

  ' ── comandi legacy: RDD MEMORY / BUILD / FIND PATH / UPDATE ──────────────
  BRAINS RDD MEMORY
  BRAINS BUILD fixture, work
  BRAINS FIND PATH "index.md" TO "trading/momentum.md"
  BRAINS UPDATE fixture, work
  PRINT "built="; IO.DIREXISTS(work)

  DIM r AS STRING

  r = BRAINS.NEIGHBORS("graph-vault", "index.md")
  PRINT "neighbors="; (INSTR(r, "Success") > 0 AND INSTR(r, "index.md") > 0)
  r = BRAINS.PATH("graph-vault", "index.md", "trading/momentum.md")
  PRINT "path="; (INSTR(r, "Success") > 0 AND INSTR(r, "trading/momentum.md") > 0)
  r = BRAINS.PAGERANK("graph-vault", 3)
  PRINT "pagerank="; (INSTR(r, "Success") > 0 AND INSTR(r, "rank") > 0)
  r = BRAINS.CONNECTED("graph-vault")
  PRINT "connected="; (INSTR(r, "Success") > 0 AND INSTR(r, "componentCount") > 0)
  r = BRAINS.STATS("graph-vault")
  PRINT "stats="; (INSTR(r, "Success") > 0 AND INSTR(r, "nodes") > 0 AND INSTR(r, "edges") > 0)
  r = BRAINS.STRONGLYCONNECTED("graph-vault")
  PRINT "strongly="; (INSTR(r, "Success") > 0 AND INSTR(r, "components") > 0)
  r = BRAINS.TOPOSORT("graph-vault")
  PRINT "toposort="; (INSTR(r, "ciclo") > 0)
  r = BRAINS.BIPARTITE("graph-vault")
  PRINT "bipartite="; (INSTR(r, "Success") > 0 AND INSTR(r, "bipartite") > 0)
  r = BRAINS.BETWEENNESS("graph-vault")
  PRINT "betweenness="; (INSTR(r, "Success") > 0 AND INSTR(r, "values") > 0)
  r = BRAINS.CLUSTERING("graph-vault", "index.md")
  PRINT "clustering="; (INSTR(r, "Success") > 0 AND INSTR(r, "clustering") > 0)
  r = BRAINS.DIAMETER("graph-vault")
  PRINT "diameter="; (INSTR(r, "Success") > 0 AND INSTR(r, "diameter") > 0)
  r = BRAINS.MST("graph-vault")
  PRINT "mst="; (INSTR(r, "Success") > 0 AND INSTR(r, "weight") > 0)
  r = BRAINS.DOT("graph-vault")
  PRINT "dot="; (LEFT(r, 7) = "digraph")
  r = BRAINS.SUBGRAPH("graph-vault", "index.md", 1)
  PRINT "subgraph="; (LEFT(r, 8) = "graph TD")
  r = BRAINS.BYTAG("graph-vault", "strategy")
  PRINT "bytag="; (INSTR(r, "strategy") > 0 AND INSTR(r, "count") > 0)
  r = BRAINS.BYFOLDER("graph-vault", "strategies")
  PRINT "byfolder="; (INSTR(r, "strategies") > 0 AND INSTR(r, "count") > 0)
  r = BRAINS.ISVALID("graph-vault")
  PRINT "isvalid="; (r = "1")
  r = BRAINS.HASCYCLE("graph-vault")
  PRINT "hascycle="; (r = "1")
  r = BRAINS.DIJKSTRA("graph-vault", "index.md", "trading/momentum.md")
  PRINT "dijkstra="; (INSTR(r, "Success") > 0 AND INSTR(r, "path") > 0)
  r = BRAINS.ECCENTRICITY("graph-vault", "index.md")
  PRINT "eccentricity="; (INSTR(r, "Success") > 0 AND INSTR(r, "eccentricity") > 0)
  r = BRAINS.DFS("graph-vault", "index.md")
  PRINT "dfs="; (INSTR(r, "Success") > 0 AND INSTR(r, "order") > 0)
  r = BRAINS.BFS("graph-vault", "index.md")
  PRINT "bfs="; (INSTR(r, "Success") > 0 AND INSTR(r, "order") > 0)
  r = BRAINS.QUERYMULTI("momentum", "graph-vault")
  PRINT "querymulti="; (INSTR(r, "Success") > 0 AND INSTR(r, "brainCount") > 0 AND INSTR(r, "momentum") > 0)
  r = BRAINS.LOUVAIN("graph-vault")
  PRINT "louvain="; (INSTR(r, "Success") > 0 AND INSTR(r, "communities") > 0)
  r = BRAINS.MAXFLOW("graph-vault", "index.md", "airline/load-factor.md")
  PRINT "maxflow="; (INSTR(r, "Success") > 0 AND INSTR(r, "flow") > 0)

  RMDIR work, TRUE
END SUB
