' R4 — VECTORDB.DIAGNOSE(eval$): analisi diagnostica dei risultati RAG.
' Test su 2 casi: ideal non recuperato → retrieval_miss, faithfulness bassa → hallucination.
' EXPECT: miss= retrieval_miss|hall= generation_hallucination
IMPORT VECTORDB

SUB Main()
    ' Caso 1: ideal non nei retrieved
    DIM e1$ = "{""retrieved"":[""d2"",""d3""],""ideal"":[""d1""],""faithfulness"":0.9,""relevance"":0.8,""hallucination"":0.1,""completeness"":0.7,""context_len"":500}"
    DIM d1$ = VECTORDB.DIAGNOSE(e1$)
    IF INSTR(d1$, "retrieval_miss") > 0 THEN PRINT "miss= retrieval_miss" ELSE PRINT "miss= ERR: " + d1$

    ' Caso 2: faithfulness bassa + hallucination alta → hallucination
    DIM e2$ = "{""retrieved"":[""d1""],""ideal"":[""d1""],""faithfulness"":0.2,""relevance"":0.5,""hallucination"":0.8,""completeness"":0.3,""context_len"":500}"
    DIM d2$ = VECTORDB.DIAGNOSE(e2$)
    IF INSTR(d2$, "generation_hallucination") > 0 THEN PRINT "hall= generation_hallucination" ELSE PRINT "hall= ERR: " + d2$
END SUB