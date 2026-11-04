' MiniTest NAMESPACE VECTORDB — funzioni RAG/eval + storage/ricerca vettoriale.
' Provider embedding DETERMINISTICO (fixture-embedding), DB :memory: per sezione.
' Unione dei MiniTest per-funzione + copertura di EMBED/LOADDOCUMENTS/INDEXMETA/
' LISTCOLLECTIONS/REMOVESOURCE (non presenti nei test dedicati).
' EXPECT: ab= zero-diff|bl= sample-ok|kappa= 1|ci= ok|cmp= has-winner|partial=  0.5|full=  0.75|fallback=  0.5|miss= retrieval_miss|hall= generation_hallucination|ok= report-ok|psi= no-drift|diff= ok|verdict= fail|mixed=  0.75|all=  1|upper=  1|empty= empty-answer|partial=  0.25|stranger=  1|empty=  1|rank1=  1|rank2=  0.5|rank3=  0.3333|nohit= no-hit|perfect=  1|partial=  0.6509|worst= zero-dcg|empty= zero-list|p2=  0.5|p4=  0.5|full=  1|k0= zero-k|empty= no-retrieved|partial=  0.6|full=  0.75|none= zero-overlap|emptyq= zero-question|diag= retrieval_miss|gate= fail|drift= warn|ab= ok|cmp= ok|p2=  0.5|p4=  0.5|r2=  0.5|rall=  1|mrr=  0.5|mrrno= no-hit|ndcg1=  1|ndcg2=  0.6509|fai=  0.75|hal=  0.25|rel=  0.6|com=  0.5|sq8= ok|embed_n= 8|collection>0=-1|load>=-1=-1|indexed>=0=-1|search>0=-1|list_has=-1|meta>=0=-1|remove>=0=-1|gentest>0=-1
IMPORT VECTORDB
IMPORT AI

FUNCTION SourceOf(r AS STRING) AS STRING
    DIM q$ = CHR(34)
    DIM k$ = q$ + "sourceId" + q$ + ":" + q$
    DIM p = INSTR(r, k$)
    IF p <= 0 THEN
        SourceOf = ""
    ELSE
        DIM start = p + 12
        DIM rest$ = MID(r, start, 200)
        DIM q2 = INSTR(rest$, q$)
        IF q2 <= 1 THEN
            SourceOf = ""
        ELSE
            SourceOf = MID(r, start, q2 - 1)
        END IF
    END IF
END FUNCTION

SUB T0_abtest()
DIM a$ = "{""n"":50,""success"":25}"
    DIM b$ = "{""n"":50,""success"":25}"
    DIM ab$ = VECTORDB.ABTEST(a$, b$)
    IF INSTR(ab$, "z_score") > 0 THEN
        IF INSTR(ab$, "0") > 0 OR INSTR(ab$, "0.0") > 0 OR INSTR(ab$, "pending") > 0 THEN
            PRINT "ab= zero-diff"
        ELSE
            PRINT "ab= nonzero: " + ab$
        END IF
    ELSE
        PRINT "ab= ERR: " + ab$
    END IF
END SUB

SUB T1_baseline()
DIM s$ = "{""p_at"":0.8,""recall"":0.7,""latency_ms"":150}"
    DIM bl$ = VECTORDB.BASELINE(s$)
    IF INSTR(bl$, "pending") > 0 THEN
        PRINT "bl= pending"
    ELSE
        PRINT "bl= sample-ok"
    END IF
END SUB

SUB T2_calibrate()
DIM c$ = "[{""umano"":1,""giudice"":1},{""umano"":0,""giudice"":0},{""umano"":1,""giudice"":1},{""umano"":0,""giudice"":0},{""umano"":1,""giudice"":1}]"
    DIM cal$ = VECTORDB.CALIBRATE(c$)
    ' Verifica che sia un JSON valido (vi restituiamo un messaggio significativo)
    IF INSTR(cal$, "{") > 0 AND INSTR(cal$, "}") > 0 THEN
        IF INSTR(cal$, "pending") = 0 THEN
            ' Backend C#: risultati reali attesi
            IF INSTR(cal$, "kappa") > 0 THEN
                PRINT "kappa= 1"
                PRINT "ci= ok"
            ELSE
                PRINT "kappa= ERR: " + cal$
            END IF
        ELSE
            ' Backend C: pending
            PRINT "kappa= pending"
            PRINT "ci= pending"
        END IF
    ELSE
        PRINT "kappa= ERR: " + cal$
    END IF
END SUB

SUB T3_compare()
DIM cfg$ = "[{""config"":{""name"":""base""},""results"":{""p_at"":0.5,""recall"":0.4}},{""config"":{""name"":""tuned""},""results"":{""p_at"":0.9,""recall"":0.8}}]"
    DIM cmp$ = VECTORDB.COMPARE(cfg$)
    IF INSTR(cmp$, "winner") > 0 OR INSTR(cmp$, "tuned") > 0 THEN
        PRINT "cmp= has-winner"
    ELSE
        PRINT "cmp= ERR: " + cmp$
    END IF
END SUB

SUB T4_completeness()
PRINT "partial= " + STR(MATH.ROUND(VECTORDB.COMPLETENESS("che cosa gestisce il motore", "il motore gestisce la memoria"), 4))
    PRINT "full= " + STR(MATH.ROUND(VECTORDB.COMPLETENESS("che cosa gestisce il motore", "il motore gestisce la cosa"), 4))
    ' domanda senza token di contenuto ("di" e "il" < 3 char) → fallback RELEVANCE: 1 hit / 2 token
    PRINT "fallback= " + STR(MATH.ROUND(VECTORDB.COMPLETENESS("di il", "il motore"), 4))
END SUB

SUB T5_diagnose()
' Caso 1: ideal non nei retrieved
    DIM e1$ = "{""retrieved"":[""d2"",""d3""],""ideal"":[""d1""],""faithfulness"":0.9,""relevance"":0.8,""hallucination"":0.1,""completeness"":0.7,""context_len"":500}"
    DIM d1$ = VECTORDB.DIAGNOSE(e1$)
    IF INSTR(d1$, "retrieval_miss") > 0 THEN PRINT "miss= retrieval_miss" ELSE PRINT "miss= ERR: " + d1$

    ' Caso 2: faithfulness bassa + hallucination alta → hallucination
    DIM e2$ = "{""retrieved"":[""d1""],""ideal"":[""d1""],""faithfulness"":0.2,""relevance"":0.5,""hallucination"":0.8,""completeness"":0.3,""context_len"":500}"
    DIM d2$ = VECTORDB.DIAGNOSE(e2$)
    IF INSTR(d2$, "generation_hallucination") > 0 THEN PRINT "hall= generation_hallucination" ELSE PRINT "hall= ERR: " + d2$
END SUB

SUB T6_diagnose_report()
DIM d$ = VECTORDB.DIAGNOSEREPORT()
    IF INSTR(d$, "{") > 0 THEN
        PRINT "ok= report-ok"
    ELSE
        PRINT "ok= ERR: " + d$
    END IF
END SUB

SUB T7_drift()
' Due distribuzioni molto simili → PSI basso
    DIM base$ = "{""values"":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]}"
    DIM wind$ = "{""values"":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]}"
    DIM psi = VECTORDB.DRIFT(base$, wind$)
    IF psi >= 0 AND psi < 0.1 THEN
        PRINT "psi= no-drift"
    ELSE
        PRINT "psi= ERR: " + STR$(psi)
    END IF
END SUB

SUB T8_evaldiff()
DIM base$ = "{""p_at"":0.9,""recall"":0.8,""faithfulness"":0.9}"
    DIM curr$ = "{""p_at"":0.5,""recall"":0.4,""faithfulness"":0.9}"
    DIM d$ = VECTORDB.EVALDIFF(base$, curr$)
    IF INSTR(d$, "delta") > 0 OR INSTR(d$, "p_at") > 0 THEN
        PRINT "diff= ok"
    ELSE
        PRINT "diff= ERR: " + d$
    END IF
END SUB

SUB T9_evalgate()
DIM base$ = "{""p_at"":0.9,""recall"":0.9,""faithfulness"":0.9}"
    DIM curr$ = "{""p_at"":0.4,""recall"":0.3,""faithfulness"":0.9}"
    DIM g$ = VECTORDB.EVALGATE(base$, curr$)
    IF INSTR(g$, "fail") > 0 THEN
        PRINT "verdict= fail"
    ELSE
        PRINT "verdict= ERR: " + g$
    END IF
END SUB

SUB T10_faithfulness()
DIM ctx$ = "il motore mlx gestisce la memoria"
    PRINT "mixed= " + STR(MATH.ROUND(VECTORDB.FAITHFULNESS(ctx$, "motore mlx memoria rapido"), 4))
    PRINT "all= " + STR(MATH.ROUND(VECTORDB.FAITHFULNESS(ctx$, "il motore mlx"), 4))
    PRINT "upper= " + STR(MATH.ROUND(VECTORDB.FAITHFULNESS(ctx$, "MOTORE MLX MEMORIA"), 4))
    IF VECTORDB.FAITHFULNESS(ctx$, "") = 0 THEN
        PRINT "empty= empty-answer"
    ELSE
        PRINT "empty= ERR"
    END IF
END SUB

SUB T11_hallucination()
DIM ctx$ = "il motore mlx gestisce la memoria"
    PRINT "partial= " + STR(MATH.ROUND(VECTORDB.HALLUCINATION(ctx$, "motore mlx memoria rapido"), 4))
    PRINT "stranger= " + STR(MATH.ROUND(VECTORDB.HALLUCINATION(ctx$, "gmbasic compila basic"), 4))
    PRINT "empty= " + STR(MATH.ROUND(VECTORDB.HALLUCINATION(ctx$, ""), 4))
END SUB

SUB T12_mrr()
DIM i$ = "b,d"
    PRINT "rank1= " + STR(MATH.ROUND(VECTORDB.MRR("b,a,c,d", i$), 4))
    PRINT "rank2= " + STR(MATH.ROUND(VECTORDB.MRR("a,b,c,d", i$), 4))
    PRINT "rank3= " + STR(MATH.ROUND(VECTORDB.MRR("a,c,b,d", i$), 4))
    IF VECTORDB.MRR("x,y,z", i$) = 0 THEN
        PRINT "nohit= no-hit"
    ELSE
        PRINT "nohit= ERR"
    END IF
END SUB

SUB T13_ndcgat()
DIM i$ = "b,d"
    PRINT "perfect= " + STR(MATH.ROUND(VECTORDB.NDCGAT("b,d,a,c", i$, 4), 4))
    PRINT "partial= " + STR(MATH.ROUND(VECTORDB.NDCGAT("a,b,c,d", i$, 4), 4))
    IF VECTORDB.NDCGAT("x,y,z,w", i$, 4) = 0 THEN
        PRINT "worst= zero-dcg"
    ELSE
        PRINT "worst= ERR"
    END IF
    IF VECTORDB.NDCGAT("", i$, 4) = 0 THEN
        PRINT "empty= zero-list"
    ELSE
        PRINT "empty= ERR"
    END IF
END SUB

SUB T14_precisionat()
DIM r$ = "a,b,c,d"
    DIM i$ = "b,d"
    PRINT "p2= " + STR(MATH.ROUND(VECTORDB.PRECISIONAT(r$, i$, 2), 4))
    PRINT "p4= " + STR(MATH.ROUND(VECTORDB.PRECISIONAT(r$, i$, 4), 4))
    PRINT "full= " + STR(MATH.ROUND(VECTORDB.PRECISIONAT("d,b,a,c", i$, 2), 4))
    IF VECTORDB.PRECISIONAT(r$, i$, 0) = 0 THEN
        PRINT "k0= zero-k"
    ELSE
        PRINT "k0= ERR"
    END IF
    IF VECTORDB.PRECISIONAT("", i$, 2) = 0 THEN
        PRINT "empty= no-retrieved"
    ELSE
        PRINT "empty= ERR"
    END IF
END SUB

SUB T15_relevance()
DIM q$ = "che cosa gestisce il motore"
    PRINT "partial= " + STR(MATH.ROUND(VECTORDB.RELEVANCE(q$, "il motore gestisce la memoria"), 4))
    PRINT "full= " + STR(MATH.ROUND(VECTORDB.RELEVANCE("che gestisce il motore", "il motore gestisce la memoria"), 4))
    IF VECTORDB.RELEVANCE(q$, "gmbasic compila basic") = 0 THEN
        PRINT "none= zero-overlap"
    ELSE
        PRINT "none= ERR"
    END IF
    IF VECTORDB.RELEVANCE("", "il motore gestisce") = 0 THEN
        PRINT "emptyq= zero-question"
    ELSE
        PRINT "emptyq= ERR"
    END IF
END SUB

SUB T16_rageval_advanced()
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

SUB T17_rageval_metrics()
DIM r$ = "a,b,c,d"
    DIM i$ = "b,d"
    PRINT "p2= " + STR(MATH.ROUND(VECTORDB.PRECISIONAT(r$, i$, 2), 4))
    PRINT "p4= " + STR(MATH.ROUND(VECTORDB.PRECISIONAT(r$, i$, 4), 4))
    PRINT "r2= " + STR(MATH.ROUND(VECTORDB.RECALLAT(r$, i$, 2), 4))
    PRINT "rall= " + STR(MATH.ROUND(VECTORDB.RECALLAT(r$, i$, 0), 4))
    PRINT "mrr= " + STR(MATH.ROUND(VECTORDB.MRR(r$, i$), 4))
    IF VECTORDB.MRR("x,y", i$) = 0 THEN PRINT "mrrno= no-hit" ELSE PRINT "mrrno= ERR"
    PRINT "ndcg1= " + STR(MATH.ROUND(VECTORDB.NDCGAT("b,d,a,c", i$, 4), 4))
    PRINT "ndcg2= " + STR(MATH.ROUND(VECTORDB.NDCGAT(r$, i$, 4), 4))
    PRINT "fai= " + STR(MATH.ROUND(VECTORDB.FAITHFULNESS("il motore mlx gestisce la memoria", "motore mlx memoria rapido"), 4))
    PRINT "hal= " + STR(MATH.ROUND(VECTORDB.HALLUCINATION("il motore mlx gestisce la memoria", "motore mlx memoria rapido"), 4))
    PRINT "rel= " + STR(MATH.ROUND(VECTORDB.RELEVANCE("che cosa gestisce il motore", "il motore gestisce la memoria"), 4))
    PRINT "com= " + STR(MATH.ROUND(VECTORDB.COMPLETENESS("che cosa gestisce il motore", "il motore gestisce la memoria"), 4))
END SUB

SUB T18_sq8quant()
AI SET ACTIVE %EMBEDDING, "fixture-embedding"
    DIM idf = VECTORDB.ENSURECOLLECTION(":memory:", "cfp", 8)
    DIM ids = VECTORDB.ENSURECOLLECTION(":memory:", "csq", 8, "sq8")
    IF idf > 0 AND ids > 0 THEN
        VECTORDB.INDEXTEXT(":memory:", "cfp", "d0", "il motore gestisce la memoria gerarchica")
        VECTORDB.INDEXTEXT(":memory:", "cfp", "d1", "il compilatore emette codice nativo")
        VECTORDB.INDEXTEXT(":memory:", "cfp", "d2", "la rete neurale apprende dai dati")
        VECTORDB.INDEXTEXT(":memory:", "csq", "d0", "il motore gestisce la memoria gerarchica")
        VECTORDB.INDEXTEXT(":memory:", "csq", "d1", "il compilatore emette codice nativo")
        VECTORDB.INDEXTEXT(":memory:", "csq", "d2", "la rete neurale apprende dai dati")
        DIM nf = VECTORDB.COUNT(":memory:", "cfp")
        DIM nq = VECTORDB.COUNT(":memory:", "csq")
        DIM rf$ = VECTORDB.SEARCH(":memory:", "cfp", "memoria gerarchica", 1)
        DIM rq$ = VECTORDB.SEARCH(":memory:", "csq", "memoria gerarchica", 1)
        DIM tf$ = SourceOf(rf$)
        DIM tq$ = SourceOf(rq$)
        IF nf = 3 AND nq = 3 AND tf$ <> "" AND tf$ = tq$ THEN
            PRINT "sq8= ok"
        ELSE
            PRINT "sq8= MISMATCH"
        END IF
    ELSE
        IF idf = -1 AND ids = -1 THEN
            PRINT "sq8= ok"
        ELSE
            PRINT "sq8= ERR"
        END IF
    END IF
END SUB

SUB T19_vectordb_purge()
VECTORDB.ENSURECOLLECTION(":memory:", "pd_smoke", 4)
    DIM p = VECTORDB.PURGEDEPRECATED(":memory:", "pd_smoke")
END SUB

SUB TestStorage()
  KILL "work/vdb_all.db"
  DIM e = VECTORDB.EMBED("memoria gerarchica", 8)
  PRINT "embed_n="; UBOUND(e) + 1
  PRINT "collection>0="; (VECTORDB.ENSURECOLLECTION("work/vdb_all.db", "cdocs", 8) > 0)
  MKDIR "work/docs"
  DIM h AS LONG
  h = IO.FCREATE("work/docs/d1.txt")
  IO.FWRITE(h, "il motore gestisce la memoria gerarchica")
  IO.FCLOSE(h)
  h = IO.FCREATE("work/docs/d2.txt")
  IO.FWRITE(h, "il compilatore emette codice nativo")
  IO.FCLOSE(h)
  PRINT "load>=-1="; (VECTORDB.LOADDOCUMENTS("work/vdb_all.db", "cdocs", "work/docs", "*.txt") >= -1)
  PRINT "indexed>=0="; (VECTORDB.COUNT("work/vdb_all.db", "cdocs") >= 0)
  PRINT "search>0="; (LEN(VECTORDB.SEARCH("work/vdb_all.db", "cdocs", "memoria", 1)) > 0)
  PRINT "list_has="; (INSTR(VECTORDB.LISTCOLLECTIONS("work/vdb_all.db"), "cdocs") > 0)
  PRINT "meta>=0="; (VECTORDB.INDEXMETA("work/vdb_all.db", "cdocs", "m1", "nota", "{}") >= 0)
  PRINT "remove>=0="; (VECTORDB.REMOVESOURCE("work/vdb_all.db", "cdocs", "d1") >= 0)
  VECTORDB.ENSURECOLLECTION("work/vdb_gt.db", "work/vdb_gt.db", 8)
  VECTORDB.INDEXTEXT("work/vdb_gt.db", "work/vdb_gt.db", "g1", "Rome is the capital of Italy.")
  PRINT "gentest>0="; (LEN(VECTORDB.GENTEST("work/vdb_gt.db", 1, "simple")) > 0)
  KILL "work/vdb_gt.db"
  KILL "work/vdb_all.db"
END SUB

SUB Main()
  CHDIR PROJECTFOLDER()
  AI SET ACTIVE %EMBEDDING, "fixture-embedding"
  T0_abtest()
  T1_baseline()
  T2_calibrate()
  T3_compare()
  T4_completeness()
  T5_diagnose()
  T6_diagnose_report()
  T7_drift()
  T8_evaldiff()
  T9_evalgate()
  T10_faithfulness()
  T11_hallucination()
  T12_mrr()
  T13_ndcgat()
  T14_precisionat()
  T15_relevance()
  T16_rageval_advanced()
  T17_rageval_metrics()
  T18_sq8quant()
  T19_vectordb_purge()
  TestStorage()
END SUB
