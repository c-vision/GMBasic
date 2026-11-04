' AI.GRAPHMULTI — query distribuita su PIÙ brain graph (R7). Costruisce due grafi
' (vault completo + solo la cartella strategies come secondo brain) e li interroga in
' unione per path/titolo con provenance del brain. Fixtures in samples/Fixtures,
' artefatti in work/ accanto a questo programma. Path da PROJECTFOLDER() (cartella
' del file .bas), mai dalla CWD di lancio.

IMPORT BRAINS
IMPORT AI WITH ALIAS

' Fixture condivisa in samples/Fixtures/graph-vault, work/ accanto al programma.
FIXTURE$ = PROJECTFOLDER() + "/../Fixtures/graph-vault"
WORK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WORK$) = 0 THEN MKDIR WORK$

' Primo brain: il vault completo (nome = basename = "graph-vault").
BRAINS BUILD FIXTURE$, WORK$

' Secondo brain: la cartella strategies come vault separato (nome = "strategies").
BRAINS BUILD PROJECTFOLDER() + "/../Fixtures/graph-vault/strategies", WORK$

' AI.GRAPHMULTI(query [, brains]) — senza brains usa tutti i brain collegati
' (AI BRAIN USE); con brains espliciti (lista separata da virgole) li usa direttamente.
PRINT AI.GRAPHMULTI("momentum", "graph-vault,strategies")
PRINT AI.GRAPHMULTI("trend-following", "graph-vault,strategies")

' Con un solo brain esplicito limita la ricerca a quello.
PRINT AI.GRAPHMULTI("index", "graph-vault")

PRINT "GraphQueryMulti ok"
