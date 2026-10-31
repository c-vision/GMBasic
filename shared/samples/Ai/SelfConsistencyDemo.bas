' ============================================================================
' SelfConsistencyDemo.bas — Demo completa AI Self-Consistency / Semantic Entropy
' Categoria: Community/Ai
' Verifica: parity 4 backend (IL, PCodeVM, VM/C, C99Native)
' ============================================================================

' ---- CONFIGURAZIONE ----
' Per AI.CONSISTENCY serve un provider LLM attivo (%CHAT).
' Se non c'è provider, la funzione restituisce NaN e invalida il dettaglio.
' AI.LEXICALENTROPY e AI.CONSISTENCYDETAIL$() sono deterministiche e
' funzionano SENZA provider (usano il runtime C condiviso).

print "============================================================"
print "  GMBasic — Self-Consistency / Semantic Entropy Demo"
print "  Algoritmo: Spanda R_sc (Shannon entropy su exact-match)"
print "  Costo: ~1.3 µs CPU, zero GPU"
print "============================================================"
print ""

' ---- 1. AI.LEXICALENTROPY: metrica pura su risposte GIÀ ottenute ----
print "--- 1. AI.LEXICALENTROPY (metrica deterministica) ---"
print ""

' Caso A: consenso unanime (rischio 0)
resp_a$ = "42;42;42;42;42"
e_a = AI.LEXICALENTROPY(resp_a$)
r_a$ = AI.CONSISTENCYDETAIL$()
print "Input:  " + resp_a$
print "Entropia: " + STR(MATH.ROUND(e_a, 6))
print "Detail:  " + r_a$
print ""

' Caso B: divergenza massima (rischio 1)
resp_b$ = "42;24;13;7;99"
e_b = AI.LEXICALENTROPY(resp_b$)
r_b$ = AI.CONSISTENCYDETAIL$()
print "Input:  " + resp_b$
print "Entropia: " + STR(MATH.ROUND(e_b, 6))
print "Detail:  " + r_b$
print ""

' Caso C: split 3-2 (rischio intermedio)
resp_c$ = "The answer is 42;42;42.0;seven;7"
e_c = AI.LEXICALENTROPY(resp_c$)
r_c$ = AI.CONSISTENCYDETAIL$()
print "Input:  " + resp_c$
print "Entropia: " + STR(MATH.ROUND(e_c, 6))
print "Detail:  " + r_c$
print ""

' Caso D: normalizzazione (co-markers, filler, numeri)
resp_d$ = "#### The answer is $42.0;Answer: 42;\boxed{42};Final answer is 42"
e_d = AI.LEXICALENTROPY(resp_d$)
r_d$ = AI.CONSISTENCYDETAIL$()
print "Input:  " + resp_d$
print "Entropia: " + STR(MATH.ROUND(e_d, 6))
print "Detail:  " + r_d$
print ""

' Caso E: mixed case + filler
resp_e$ = "THE ANSWER IS 42;the answer is 42;The Answer Is 42;42;forty two"
e_e = AI.LEXICALENTROPY(resp_e$)
r_e$ = AI.CONSISTENCYDETAIL$()
print "Input:  " + resp_e$
print "Entropia: " + STR(MATH.ROUND(e_e, 6))
print "Detail:  " + r_e$
print ""

' ---- 2. Interpretazione risk_score ----
print "--- 2. Interpretazione risk_score ---"
print ""
print "risk = 0.0     → consenso unanime (affidabile)"
print "risk < 0.3     → accordo forte"
print "risk 0.3-0.7   → divergenza moderata"
print "risk > 0.7     → divergenza forte (modello incerto)"
print "risk ≈ 1.0     → tutte le risposte diverse"
print ""
print "⚠️  Heuristic: risk < 0.05 con K>=3 → collapse_warning=true"
print "    (possibile Confident Mode Collapse su modelli >100B)"
print ""

' ---- 3. AI.CONSISTENCY: campionamento K× da provider LLM ----
print "--- 3. AI.CONSISTENCY (richiede provider %CHAT REALE) ---"
print ""
print "Nota: AI.CONSISTENCY campiona K risposte dallo stesso modello (AskMulti)"
print "      e calcola il risk. Senza provider REALE (il fixture di bootstrap non"
print "      conta) restituisce NaN e invalida il dettaglio: stesso esito su 5/5."
print ""

' Chiamata reale: con provider reale → risk 0..1; senza → NaN.
risk = AI.CONSISTENCY("Qual è la capitale dell'Australia?")
if risk <> risk then
    print "[NESSUN PROVIDER REALE: AI.CONSISTENCY = NaN]"
    print "  Configura un provider con: AI SET ACTIVE %CHAT, nome_provider"
    print "  Detail: " + AI.CONSISTENCYDETAIL$()
else
    print "Risk:   " + STR(risk)
    print "Detail: " + AI.CONSISTENCYDETAIL$()
    if risk < 0.3 then
        print "→ CONSENSO FORTE (affidabile)"
    elseif risk < 0.7 then
        print "→ DIVERGENZA MODERATA (verifica consigliata)"
    else
        print "→ DIVERGENZA FORTE (escalate a RAG/modello più grande)"
    endif
endif

' Opzioni esplicite: K=3, temp=0.5
risk3 = AI.CONSISTENCY("2+2=?", {k: 3, temp: 0.5})
if risk3 <> risk3 then
    print "Con opzioni (K=3,T=0.5): NaN (nessun provider)"
else
    print "Con opzioni (K=3,T=0.5): risk=" + STR(risk3)
endif

print ""
print "============================================================"
print "  Demo completata"
print "============================================================"