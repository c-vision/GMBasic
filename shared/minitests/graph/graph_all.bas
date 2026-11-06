' MiniTest NAMESPACE BRAINS — le 8 funzioni AI/accesso del cervello + i comandi del dominio.
'
' Superficie (shared/spec/registry/surface.csv): BYFOLDER, BYTAG, DOT, ISVALID, NEIGHBORS,
' PATH, QUERYMULTI, STATS. La teoria dei grafi vive nel pacchetto FFI GRAPH/FLOW
' (shared/minitests/graphffi/*, samples/graph/GraphFFILab.bas): qui non si duplica.
' Comandi coperti: BRAINS RDD MEMORY, BRAINS BUILD, BRAINS UPDATE, BRAINS FIND PATH.
'
' Fixture: shared/samples/Fixtures/graph-vault (5 nodi, 11 archi).
' Scopo: verificare che i 4 backend supportino la STESSA versione del namespace,
' non un'applicazione. Si asseriscono FATTI (status e campi), non il JSON intero.
' EXPECT: built=-1|neighbors=-1|path=-1|stats=-1|dot=-1|bytag=-1|byfolder=-1|isvalid=-1|querymulti=-1
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
  r = BRAINS.STATS("graph-vault")
  PRINT "stats="; (INSTR(r, "Success") > 0 AND INSTR(r, "nodes") > 0 AND INSTR(r, "edges") > 0)
  r = BRAINS.DOT("graph-vault")
  PRINT "dot="; (LEFT(r, 7) = "digraph")
  r = BRAINS.BYTAG("graph-vault", "strategy")
  PRINT "bytag="; (INSTR(r, "strategy") > 0 AND INSTR(r, "count") > 0)
  r = BRAINS.BYFOLDER("graph-vault", "strategies")
  PRINT "byfolder="; (INSTR(r, "strategies") > 0 AND INSTR(r, "count") > 0)
  r = BRAINS.ISVALID("graph-vault")
  PRINT "isvalid="; (r = "1")
  r = BRAINS.QUERYMULTI("momentum", "graph-vault")
  PRINT "querymulti="; (INSTR(r, "Success") > 0 AND INSTR(r, "brainCount") > 0 AND INSTR(r, "momentum") > 0)

  ' ── le funzioni di teoria dei grafi NON esistono più in BRAINS ───────────
  ' (i test FFI corrispondenti stanno in shared/minitests/graphffi/)
  RMDIR work, TRUE
END SUB
