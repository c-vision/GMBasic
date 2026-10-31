' Self-Consistency / Semantic Entropy (Spanda R_sc) — MiniTest
' Oracolo per funzione (EXPECT: commento, non eseguibile):
' AI.LEXICALENTROPY: identiche→0.0, tutte_diverse→log2(5)≈2.321928, split3-2→~0.970951, normalizzazione→0.0, mixed→~0.721928, K=1→0.0
' AI.CONSISTENCYDETAIL$(): detail contiene k, n_buckets, entropy, risk, collapse_warning, buckets
' AI.CONSISTENCY: senza provider → NaN (non testabile qui)
'
' EXPECT: e1=0|e2=2.3219|e3=0.971|e4=0|e5=0.7219|e6=0|e7=0.9183|detail1={"k":3,"n_buckets":2,"entropy":0.918296,"risk":0.579380,"collapse_warning":false,"buckets":[{"size":2,"repr":"42"},{"size":1,"repr":""}]}|detail10={"k":5,"n_buckets":1,"entropy":0.000000,"risk":0.000000,"collapse_warning":true,"buckets":[{"size":5,"repr":"42"}]}|risk_nan=1|detail_after_consistency={}

IMPORT AI

SUB Main()
    ' ===== AI.LEXICALENTROPY =====
    ' Test 1: risposte identiche → entropia 0
    e1 = AI.LEXICALENTROPY("42;42;42;42;42")
    PRINT "e1=" + LTRIM$(STR(MATH.ROUND(e1, 4)))

    ' Test 2: tutte diverse → entropia massima = log2(5) ≈ 2.321928
    e2 = AI.LEXICALENTROPY("42;24;13;7;99")
    PRINT "e2=" + LTRIM$(STR(MATH.ROUND(e2, 4)))

    ' Test 3: split 3-2 → entropia = -(0.6*log2(0.6) + 0.4*log2(0.4)) ≈ 0.970951
    e3 = AI.LEXICALENTROPY("The answer is 42;42;42.0;seven;7")
    PRINT "e3=" + LTRIM$(STR(MATH.ROUND(e3, 4)))

    ' Test 4: normalizzazione (co-markers, filler, numeri)
    e4 = AI.LEXICALENTROPY("#### The answer is $42.0;Answer: 42;\boxed{42};Final answer is 42")
    PRINT "e4=" + LTRIM$(STR(MATH.ROUND(e4, 4)))

    ' Test 5: mixed case + filler
    e5 = AI.LEXICALENTROPY("THE ANSWER IS 42;the answer is 42;The Answer Is 42;42;forty two")
    PRINT "e5=" + LTRIM$(STR(MATH.ROUND(e5, 4)))

    ' Test 6: K=1 edge case
    e6 = AI.LEXICALENTROPY("42")
    PRINT "e6=" + LTRIM$(STR(MATH.ROUND(e6, 4)))

    ' Test 7: stringhe vuote
    e7 = AI.LEXICALENTROPY("42;;42")
    PRINT "e7=" + LTRIM$(STR(MATH.ROUND(e7, 4)))

    ' ===== AI.CONSISTENCYDETAIL$() =====
    ' Detail dopo test 3 (split 3-2)
    d1$ = AI.CONSISTENCYDETAIL$()
    PRINT "detail1=" + d1$

    ' Detail dopo test 1 (identiche, K=5) → collapse_warning=true
    e10 = AI.LEXICALENTROPY("42;42;42;42;42")
    detail10$ = AI.CONSISTENCYDETAIL$()
    PRINT "detail10=" + detail10$

    ' ===== AI.CONSISTENCY =====
    ' Senza provider LLM REALE (il "fixture" di bootstrap NON conta) → NaN e dettaglio
    ' invalidato "{}". Parità: i backend C non hanno provider in-process → NaN; i backend
    ' gestiti rigettano il fixture → NaN. Stesso esito su 5/5.
    DIM risk AS DOUBLE = AI.CONSISTENCY("Qual è la capitale dell'Australia?")
    PRINT "risk_nan=" + IIF(risk <> risk, "1", "0")
    PRINT "detail_after_consistency=" + AI.CONSISTENCYDETAIL$()

END SUB
