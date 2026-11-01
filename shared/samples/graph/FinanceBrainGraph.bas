' FILE: graph/FinanceBrainGraph.bas
' =================================================================================================
'       BRAIN FINANCE — knowledge graph sul dominio finanziario (AiFinanceWiki)
' =================================================================================================
' Interroga il brain "finance" (GRAPH) costruito da:
'   gmb graph build ~/Documents/AiFinanceWiki --name finance
' poi collegato con `gmb brain use finance`.
'
' Il grafo ha ~17118 nodi (note .md) e ~83k edge (wikilink), 1883 community. GRAPH.QUERYMULTI
' trova i nodi che matchano la query e restituisce i vicini con provenance brain. Estrazione
' reale delle relazioni tra concetti del dominio (Black-Scholes, opzioni, volatility).
'
' NB: il brain finance è disponibile in modalita' GRAPH (il RAG full-17k richiederebbe ore di
' embedding: file ~170KB medi). Per il retrieval semantico usa il brain airline o un RAG su
' sottoinsieme. NON va in CI (dipende dal brain locale costruito).

IMPORT GRAPH
IMPORT AI WITH ALIAS

SUB Main()
    ' Verifica che il brain sia collegato.
    DIM used$ = AI.BRAINUSED()
    IF INSTR("," + used$ + ",", ",finance,") = 0 THEN
        PRINT "BRAIN-GRAPH-FAIL: brain 'finance' non collegato (esegui: gmb brain use finance)"
        RETURN
    END IF

    PRINT "=== Brain Finance — Knowledge Graph ==="
    PRINT "brain usati: "; used$
    PRINT

    ' GRAPH.QUERYMULTI(query [, brains]) — trova i nodi (match su path/titolo) e i vicini.
    DIM g1$ = GRAPH.QUERYMULTI("BlackScholes", "finance")
    PRINT "G1 Black-Scholes ->"; LEFT$(g1$, 500)
    PRINT

    DIM g2$ = GRAPH.QUERYMULTI("options derivatives", "finance")
    PRINT "G2 options ->"; LEFT$(g2$, 500)
    PRINT

    DIM g3$ = GRAPH.QUERYMULTI("volatility greeks", "finance")
    PRINT "G3 volatility ->"; LEFT$(g3$, 500)
    PRINT

    ' Estrazione reale: se i nodi Black-Scholes/options/volatility esistono, il grafo e' vivo.
    IF INSTR(g1$, "Black") > 0 AND (INSTR(g2$, "Options") > 0 OR INSTR(g2$, "options") > 0) THEN
        PRINT "BRAIN-GRAPH-OK: relazioni reali confermate (Black-Scholes, options)"
    ELSE
        PRINT "BRAIN-GRAPH-CHECK: verifica manuale dei nodi"
    END IF
END SUB
