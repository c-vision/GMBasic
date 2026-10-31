' GRAPH BUILD / GRAPH RDD / GRAPH FIND PATH — comandi legacy del namespace GRAPH (L5).
' Costruisce il knowledge graph arricchito (8 campi + edge tipizzati weighted) da un vault .md.
'
' Protocollo file (§9.3/§11 AGENTS.md):
'   - INPUT (fixture condivisa)   : samples/Fixtures/graph-vault   (mai modificata)
'   - OUTPUT (artefatti generati) : work/  accanto a questo programma (creata qui)
' I path si derivano SEMPRE da PROJECTFOLDER() (cartella del file .bas, non la CWD di lancio).

IMPORT GRAPH

' PROJECTFOLDER() = cartella assoluta di questo file (es. .../samples/graph). La fixture
' condivisa vive in samples/Fixtures/graph-vault (una cartella sopra il sample).
FIXTURE$ = PROJECTFOLDER() + "/../Fixtures/graph-vault"
' Gli artefatti (export JSON del grafo) finiscono in work/ accanto a questo programma.
WORK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WORK$) = 0 THEN MKDIR WORK$

' GRAPH RDD MEMORY — backend in-memory (default). Alternativa: JSON (persiste su work/<nome>.graph.json).
GRAPH RDD MEMORY

' GRAPH BUILD <dir> [, outputDir] — costruisce il grafo arricchito (8 campi + edge tipizzati)
' e scrive l'export standard work/vault.json per le funzioni GRAPH.NEIGHBORS/PATH/...
GRAPH BUILD FIXTURE$, WORK$

' GRAPH FIND PATH <from> TO <to> — path più corto (BFS) nel grafo attivo.
GRAPH FIND PATH "index.md" TO "trading/momentum.md"
GRAPH FIND PATH "index.md" TO "airline/load-factor.md"

' GRAPH RDD JSON — persiste anche il grafo arricchito (work/vault.graph.json).
GRAPH RDD JSON
GRAPH BUILD FIXTURE$, WORK$

' GRAPH RDD SQLITE — persiste il grafo arricchito in un database SQLite (work/vault.graph.db,
' tabelle graph_nodes/graph_edges). ISAM/DBF richiedono driver proprietari (Commercial Edition)
' → errore esplicito.
GRAPH RDD SQLITE
GRAPH BUILD FIXTURE$, WORK$
GRAPH FIND PATH "index.md" TO "trading/momentum.md"

' GRAPH UPDATE <dir> [, outputDir] — alias di BUILD (ricostruisce il grafo).
GRAPH UPDATE FIXTURE$, WORK$
GRAPH RDD MEMORY

PRINT "GraphBuild ok (fixture: " + FIXTURE$ + ", work: " + WORK$ + ")"
