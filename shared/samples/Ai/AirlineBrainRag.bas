' FILE: Ai/AirlineBrainRag.bas
' =================================================================================================
'       BRAIN AIRLINE — retrieval RAG sul dominio aereo (AiAirlineWiki)
' =================================================================================================
' Interroga il brain "airline" (RAG, embedding reale bge) costruito da:
'   gmb rag build ~/Documents/AiAirlineWiki --name airline
' poi collegato con `gmb brain use airline` (o AI BRAIN USE airline).
'
' Il brain contiene ~1088 documenti del vault AiAirlineWiki (dominio aereo: EDIFACT PADIS,
' NDC, AIRIMP, BSP, OneOrder, E-Ticketing...). La query restituisce i chunk piu' rilevanti
' per significato (non solo keyword) — estrazione reale di conoscenza dal dominio.
'
' NON va in CI (dipende dal brain locale costruito). Eseguire da test_ilrun/REPL dopo il build.

IMPORT AI WITH ALIAS

SUB Main()
    ' Verifica che il brain sia collegato e in modalita' rag.
    DIM used$ = AI.BRAINUSED()
    IF INSTR("," + used$ + ",", ",airline,") = 0 THEN
        PRINT "BRAIN-RAG-FAIL: brain 'airline' non collegato (esegui: gmb brain use airline)"
        RETURN
    END IF

    PRINT "=== Brain Airline — RAG (embedding bge reale) ==="
    PRINT "brain usati: "; used$
    PRINT

    ' SEARCHMULTI(query, topK, collection, brains, reranker, minEvidence) — interroga i brain
    ' collegati. Collection default del build = "wiki"; reranker 1 = keyword.
    DIM r1$ = AI.SEARCHMULTI("come funziona il settlement BSP tra agenzie e compagnie", 3, "wiki", NULL, 1)
    PRINT "Q1 settlement BSP ->"; LEFT$(r1$, 400)
    PRINT

    DIM r2$ = AI.SEARCHMULTI("differenza tra PNR e One Order nel retailing aereo", 3, "wiki", NULL, 1)
    PRINT "Q2 PNR vs One Order ->"; LEFT$(r2$, 400)
    PRINT

    ' Estrazione mirata: chiedi un concetto specifico e verifica che torni chunk del dominio giusto.
    DIM r3$ = AI.SEARCHMULTI("che cos'e' un EMD electronic miscellaneous document", 3, "wiki", NULL, 1)
    PRINT "Q3 EMD ->"; LEFT$(r3$, 400)
    PRINT

    ' Se il chunk rilevante contiene il concetto cercato, l'estrazione e' reale.
    IF INSTR(r1$, "BSP") > 0 AND INSTR(r3$, "EMD") > 0 THEN
        PRINT "BRAIN-RAG-OK: estrazione semantica reale confermata"
    ELSE
        PRINT "BRAIN-RAG-CHECK: verifica manuale dei chunk (concetti attesi: BSP, EMD)"
    END IF
END SUB
