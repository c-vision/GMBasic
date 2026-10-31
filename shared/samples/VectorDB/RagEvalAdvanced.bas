' Sample RAG Eval — Advanced diagnostics (R4-R8).
' Dimostra tutte le funzioni avanzate di valutazione RAG:
' DIAGNOSE, DIAGNOSEREPORT, CALIBRATE, EVALGATE, EVALDIFF,
' BASELINE, DRIFT, ABTEST, COMPARE.
' Tutte le funzioni sono pure (math/logic, nessun provider embedding/chat richiesto).
IMPORT VECTORDB
IMPORT STRINGS

SUB Main()
    PRINT "=== RAG Eval Advanced (R4-R8) ==="
    PRINT ""

    ' ── R4: diagnose ─────────────────────────────
    ' Caso retrieval miss: ideal non recuperato
    DIM eval1$ = "{""retrieved"":[""d2"",""d3""],""ideal"":[""d1""],""faithfulness"":0.9,""relevance"":0.7,""hallucination"":0.1,""completeness"":0.6,""context_len"":400}"
    DIM diag1$ = VECTORDB.DIAGNOSE(eval1$)
    PRINT "R4a DIAGNOSE (retrieval_miss):"
    PRINT diag1$
    PRINT ""

    ' Caso generazione incomplete
    DIM eval2$ = "{""retrieved"":[""d1"",""d2""],""ideal"":[""d1"",""d2""],""faithfulness"":0.8,""relevance"":0.5,""hallucination"":0.1,""completeness"":0.2,""context_len"":400}"
    DIM diag2$ = VECTORDB.DIAGNOSE(eval2$)
    PRINT "R4b DIAGNOSE (incomplete): " + STRING$(1, 0)
    IF INSTR(diag2$, "incomplete") > 0 THEN
        PRINT "  -> completeness bassa rilevata"
    ELSE
        PRINT "  -> " + diag2$
    END IF
    PRINT ""

    ' ── R4: report aggregato ──────────────────────
    DIM report$ = VECTORDB.DIAGNOSEREPORT()
    PRINT "R4c DIAGNOSEREPORT:"
    PRINT report$
    PRINT ""

    ' ── R5: calibrate (kappa + Wilson) ────────────
    ' 8 casi con accordo perfetto → kappa=1
    DIM cases$ = "[{""umano"":1,""giudice"":1},{""umano"":0,""giudice"":0},{""umano"":1,""giudice"":1},{""umano"":0,""giudice"":0},{""umano"":1,""giudice"":1},{""umano"":0,""giudice"":0},{""umano"":1,""giudice"":1},{""umano"":0,""giudice"":0}]"
    DIM cal$ = VECTORDB.CALIBRATE(cases$)
    PRINT "R5 CALIBRATE:"
    PRINT cal$
    PRINT ""

    ' ── R6: evalgate ─────────────────────────────
    ' Baseline buona, corrente degradata → fail
    DIM base$ = "{""p_at"":0.9,""recall"":0.9,""faithfulness"":0.9}"
    DIM curr$ = "{""p_at"":0.3,""recall"":0.2,""faithfulness"":0.8}"
    DIM gate$ = VECTORDB.EVALGATE(base$, curr$)
    PRINT "R6a EVALGATE (regressione):"
    PRINT gate$
    PRINT ""

    ' ── R6: evaldiff ─────────────────────────────
    DIM diff$ = VECTORDB.EVALDIFF(base$, curr$)
    PRINT "R6b EVALDIFF:"
    PRINT diff$
    PRINT ""

    ' ── R7: baseline ─────────────────────────────
    DIM sample$ = "{""p_at"":0.8,""recall"":0.7,""latency_ms"":150}"
    DIM bl$ = VECTORDB.BASELINE(sample$)
    PRINT "R7a BASELINE: " + bl$
    PRINT ""

    ' ── R7: drift (PSI) ──────────────────────────
    ' Baseline omogenea vs window stabile → basso drift
    DIM b$ = "{""values"":[" + STR$(1) + "," + STR$(2) + "," + STR$(3) + "," + STR$(4) + "," + STR$(5) + "]"
    DIM b2$ = b$ + "," + STR$(6) + "," + STR$(7) + "," + STR$(8) + "," + STR$(9) + "," + STR$(10) + "]}"
    ' Window con spostamento verso alti valori → drift rilevato
    DIM w$ = "{""values"":[" + STR$(8) + "," + STR$(9) + "," + STR$(10) + "," + STR$(11) + "," + STR$(12) + "]"
    DIM w2$ = w$ + "," + STR$(13) + "," + STR$(14) + "," + STR$(15) + "," + STR$(16) + "," + STR$(17) + "]}"
    DIM psi = VECTORDB.DRIFT(b2$, w2$)
    PRINT "R7b DRIFT (PSI): " + STR$(psi)
    IF psi >= 0.25 THEN
        PRINT "  -> drift critico"
    ELSEIF psi >= 0.1 THEN
        PRINT "  -> drift in warning"
    ELSE
        PRINT "  -> nessun drift"
    END IF
    PRINT ""

    ' ── R7: abtest ──────────────────────────────
    DIM va$ = "{""n"":100,""success"":60}"
    DIM vb$ = "{""n"":100,""success"":40}"
    DIM ab$ = VECTORDB.ABTEST(va$, vb$)
    PRINT "R7c ABTEST: " + ab$
    PRINT ""

    ' ── R8: compare ─────────────────────────────
    DIM cfg$ = "[{""config"":{""name"":""base""},""results"":{""p_at"":0.5,""recall"":0.4,""ndcg"":0.3}},{""config"":{""name"":""tuned""},""results"":{""p_at"":0.9,""recall"":0.8,""ndcg"":0.7}}]"
    DIM cmp$ = VECTORDB.COMPARE(cfg$)
    PRINT "R8 COMPARE:"
    PRINT cmp$
    PRINT ""

    PRINT "=== Fine sample RAG Eval Advanced ==="
END SUB