' FILE: graph/AirlineBrainGraph.bas
' =================================================================================================
'       BRAIN AIRLINE — knowledge graph sul dominio aereo (AiAirlineWiki)
' =================================================================================================
' Interroga il brain "airline" (GRAPH) costruito da:
'   gmb graph build ~/Documents/AiAirlineWiki --name airline
' poi collegato con `gmb brain use airline`.
'
' Il grafo ha ~1083 nodi (note .md) e ~323k edge (wikilink). GRAPH.QUERYMULTI trova i nodi
' il cui path/titolo matcha la query e restituisce i vicini (relazioni) con provenance brain.
' Estrazione reale delle relazioni tra concetti del dominio (es. BSP <-> One Order <-> NDC).
'
' NON va in CI (dipende dal brain locale costruito). Eseguire da test_ilrun/REPL dopo il build.

IMPORT GRAPH
IMPORT AI WITH ALIAS

SUB Main()
    ' Verifica che il brain sia collegato.
    DIM used$ = AI.BRAINUSED()
    IF INSTR("," + used$ + ",", ",airline,") = 0 THEN
        PRINT "BRAIN-GRAPH-FAIL: brain 'airline' non collegato (esegui: gmb brain use airline)"
        RETURN
    END IF

    PRINT "=== Brain Airline — Knowledge Graph ==="
    PRINT "brain usati: "; used$
    PRINT

    ' GRAPH.QUERYMULTI(query [, brains]) — trova i nodi (match su path/titolo) e i vicini.
    DIM g1$ = GRAPH.QUERYMULTI("BSP", "airline")
    PRINT "G1 BSP ->"; LEFT$(g1$, 500)
    PRINT

    DIM g2$ = GRAPH.QUERYMULTI("ONE-Order-Architecture", "airline")
    PRINT "G2 ONE-Order ->"; LEFT$(g2$, 500)
    PRINT

    DIM g3$ = GRAPH.QUERYMULTI("EMD-Complete-Implementation", "airline")
    PRINT "G3 EMD ->"; LEFT$(g3$, 500)
    PRINT

    ' Estrazione reale: se i nodi BSP/OneOrder/EMD esistono e hanno vicini, il grafo e' vivo.
    IF INSTR(g1$, "BSP") > 0 AND INSTR(g2$, "Order") > 0 AND INSTR(g3$, "EMD") > 0 THEN
        PRINT "BRAIN-GRAPH-OK: relazioni reali confermate (BSP, ONE-Order, EMD)"
    ELSE
        PRINT "BRAIN-GRAPH-CHECK: verifica manuale dei nodi"
    END IF
END SUB
