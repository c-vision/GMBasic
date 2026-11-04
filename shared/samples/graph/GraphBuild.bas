' BRAINS BUILD / BRAINS RDD / BRAINS FIND PATH — comandi legacy del namespace BRAINS (ex GRAPH, L5).
' Costruisce il knowledge graph arricchito (8 campi + edge tipizzati weighted) da un vault .md.
'
' Protocollo file (§9.3/§11 AGENTS.md):
'   - INPUT (fixture condivisa)   : samples/Fixtures/graph-vault   (mai modificata)
'   - OUTPUT (artefatti generati) : work/  accanto a questo programma (creata qui)
' I path si derivano SEMPRE da PROJECTFOLDER() (cartella del file .bas, non la CWD di lancio).

IMPORT BRAINS

' PROJECTFOLDER() = cartella assoluta di questo file (es. .../samples/graph). La fixture
' condivisa vive in samples/Fixtures/graph-vault (una cartella sopra il sample).
FIXTURE$ = PROJECTFOLDER() + "/../Fixtures/graph-vault"
' Gli artefatti (export JSON del grafo) finiscono in work/ accanto a questo programma.
WORK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WORK$) = 0 THEN MKDIR WORK$

' BRAINS RDD MEMORY — backend in-memory (default). Alternativa: JSON (persiste su work/<nome>.graph.json).
BRAINS RDD MEMORY

' BRAINS BUILD <dir> [, outputDir] — costruisce il grafo arricchito (8 campi + edge tipizzati)
' e scrive l'export standard work/vault.json per le funzioni BRAINS.NEIGHBORS/PATH/...
BRAINS BUILD FIXTURE$, WORK$

' BRAINS FIND PATH <from> TO <to> — path più corto (BFS) nel grafo attivo.
BRAINS FIND PATH "index.md" TO "trading/momentum.md"
BRAINS FIND PATH "index.md" TO "airline/load-factor.md"

' BRAINS RDD JSON — persiste anche il grafo arricchito (work/vault.graph.json).
BRAINS RDD JSON
BRAINS BUILD FIXTURE$, WORK$

' BRAINS RDD SQLITE — persiste il grafo arricchito in un database SQLite (work/vault.graph.db,
' tabelle graph_nodes/graph_edges). ISAM/DBF non sono supportati da questo percorso RDD
' → errore esplicito.
BRAINS RDD SQLITE
BRAINS BUILD FIXTURE$, WORK$
BRAINS FIND PATH "index.md" TO "trading/momentum.md"

' BRAINS UPDATE <dir> [, outputDir] — alias di BUILD (ricostruisce il grafo).
BRAINS UPDATE FIXTURE$, WORK$
BRAINS RDD MEMORY

PRINT "GraphBuild ok (fixture: " + FIXTURE$ + ", work: " + WORK$ + ")"
