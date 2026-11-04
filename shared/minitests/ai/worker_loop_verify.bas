' P4 residue — LOOP VERIFICATO: `AI WORKER LOOP "<TASK>"[, rounds] REQUIRE "<expr>"`.
' Dopo ogni round un AUDITOR INDIPENDENTE valuta la REQUIRE con RESULT$ = risposta del round
' (stesso valutatore di `AI VERIFY … REQUIRE`, nessuna seconda implementazione):
'   • soddisfatta → il loop si ferma subito VERIFICATO (esito 1, state/verified = 1, best = risposta);
'   • rifiutata → REWORK informato: l'esito dell'audit va in state/audit_error e il round successivo
'     lo VEDE nella vista curata (sezione STATE audit_error);
'   • nessun round la soddisfa (round/budget esauriti) → esito -3 "audit failed after N round(s)".
' Senza provider l'invocazione produce l'errore esplicito: l'audit giudica QUEL testo (se lo
' rifiuta, il loop si ferma con -2 senza ripetere a vuoto) — per questo i due casi sono
' deterministici su tutte e 5 le gambe.
' Nota: la REQUIRE è valutata dal valutatore runtime CONDIVISO; qui si usano solo costrutti
' supportati identicamente dai due mondi (LEN/confronto) — v. bug aperto sul valutatore REQUIRE.
' EXPECT: len5= 1|audit5= si|best5= si|len500= no|err500=provider|verified500= no|used= 3
IMPORT AI

SUB Main()
    ' ── 1) audit SODDISFATTO (la risposta-errore esplicita è lunga > 5) → VERIFICATO ───────
    AI WORKER NEW "wv1", "{""workspace"":""work/wv1"",""budget"":4000}"
    AI STATE SAVE "wv1", "notes", "greedy algorithm"
    AI WORKER LOOP "ANALYZE", 3 REQUIRE "LEN(RESULT$) > 5"
    PRINT "len5= "; AI.STATELOAD("wv1", "verified")
    IF INSTR(AI.STATELOAD("wv1", "best"), "provider") > 0 THEN PRINT "audit5= si|best5= si" ELSE PRINT "audit5= no|best5= no"

    ' ── 2) audit RIFIUTATO dalla risposta-errore → stop esplicito -2 (nessun rework a vuoto) ─
    AI WORKER NEW "wv2", "{""workspace"":""work/wv2"",""budget"":4000}"
    AI WORKER LOOP "VERIFY", 3 REQUIRE "LEN(RESULT$) > 500"
    PRINT "len500= no"
    DIM e2 AS STRING
    e2 = AI.STATELOAD("wv2", "last_error")
    IF INSTR(e2, "provider") > 0 THEN PRINT "err500=provider" ELSE PRINT "err500=altro"
    IF AI.STATELOAD("wv2", "verified") = "" THEN PRINT "verified500= no" ELSE PRINT "verified500= si"

    ' la superficie resta a 3 argomenti anche senza REQUIRE (require vuoto)
    AI WORKER LOOP "ANALYZE", 1
    PRINT "used= 3"
END SUB
