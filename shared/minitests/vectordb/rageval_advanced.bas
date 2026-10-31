' RAG eval — Fase B/C: DIAGNOSE, EVALGATE, DRIFT, ABTEST, COMPARE.
' Funzioni pure (nessun embedding/LLM richiesto): test deterministici.
' EXPECT: diag= retrieval_miss|gate= fail|drift= warn|ab= ok|cmp= ok
IMPORT VECTORDB

SUB Main()
    ' R4 — DIAGNOSE: eval JSON con retrieval_miss (ideal non recuperato)
    DIM eval_not$ = "{""retrieved"":[""d2"",""d3""],""ideal"":[""d1""],""faithfulness"":0.8,""relevance"":0.7,""hallucination"":0.1,""completeness"":0.6,""context_len"":500}"
    DIM diag$ = VECTORDB.DIAGNOSE(eval_not$)
    IF INSTR(diag$, "retrieval_miss") > 0 THEN
        PRINT "diag= retrieval_miss"
    ELSE
        PRINT "diag= ERR: " + diag$
    END IF

    ' R6 — EVALGATE: baseline buona, corrente con drop retrieval → fail
    DIM base$ = "{""retrieved"":[1,2,3,4,5],""ideal"":[1,2,3],""p_at"":0.9,""recall"":0.9,""faithfulness"":0.9}"
    DIM curr$ = "{""retrieved"":[1,2,3,4,5],""ideal"":[1,6,7],""p_at"":0.4,""recall"":0.3,""faithfulness"":0.9}"
    DIM gate$ = VECTORDB.EVALGATE(base$, curr$)
    IF INSTR(gate$, "fail") > 0 THEN
        PRINT "gate= fail"
    ELSE
        PRINT "gate= ERR: " + gate$
    END IF

    ' R7 — DRIFT/PSI: distribuzioni con drift medio → warning (0.1-0.25)
    DIM base2$ = "{""values"":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]}"
    DIM wind$ = "{""values"":[1,1,1,2,2,2,3,3,3,10,11,12,13,14,15,16,17,18,19,20]}"
    DIM psi = VECTORDB.DRIFT(base2$, wind$)
    IF psi >= 0 THEN
        PRINT "drift= warn"
    ELSE
        PRINT "drift= ERR: " + STR$(psi)
    END IF

    ' R7 — ABTEST: due proporzioni con differenza → z-test
    DIM a$ = "{""n"":100,""success"":60}"
    DIM b$ = "{""n"":100,""success"":40}"
    DIM ab$ = VECTORDB.ABTEST(a$, b$)
    IF INSTR(ab$, "z_score") > 0 THEN
        PRINT "ab= ok"
    ELSE
        PRINT "ab= ERR: " + ab$
    END IF

    ' R8 — COMPARE: due config con metriche
    DIM cfg$ = "[{""config"":{""rerank"":""off""},""results"":{""p_at"":0.5,""recall"":0.4,""ndcg"":0.3}},{""config"":{""rerank"":""on""},""results"":{""p_at"":0.8,""recall"":0.7,""ndcg"":0.6}}]"
    DIM cmp$ = VECTORDB.COMPARE(cfg$)
    IF INSTR(cmp$, "winner") > 0 THEN
        PRINT "cmp= ok"
    ELSE
        PRINT "cmp= ERR: " + cmp$
    END IF
END SUB