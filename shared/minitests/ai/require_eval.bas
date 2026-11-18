' FILE: minitests/ai/require_eval.bas
' Valutatore del REQUIRE (AI VERIFY … REQUIRE / AI WORKER LOOP … REQUIRE): oracolo dichiarato
' per i costrutti che divergevano fra C# e C.
'   - `=`  → UGUAGLIANZA (non assegnazione): `RESULT$ = "zzz"` è FALSO (il testo non è "zzz").
'   - `INSTR(hay, needle)` → posizione 1-based (0 se assente), case-insensitive.
' Senza provider il round produce un errore ESPLICITO che cita "provider" → i casi sono
' deterministici su tutte e 5 le gambe.
' EXPECT: eq-zzz= 1|instr= 1|len5= 1
IMPORT AI

SUB Main()
  CHDIR PROJECTFOLDER()
    ' ── 1) `=` è UGUAGLIANZA: la risposta NON è "zzz" → audit rifiutato, nessun verificato ──
    AI WORKER NEW "wev1", "{""workspace"":""work/wev1"",""budget"":4000}"
    AI WORKER LOOP "VERIFY", 2 REQUIRE "RESULT$ = ""zzz"""
    PRINT "eq-zzz= "; AI.STATELOAD("wev1", "verified")

    ' ── 2) INSTR esiste nel valutatore: l'errore esplicito cita "provider" → audit soddisfatto ─
    AI WORKER NEW "wev2", "{""workspace"":""work/wev2"",""budget"":4000}"
    AI WORKER LOOP "ANALYZE", 2 REQUIRE "INSTR(RESULT$, ""provider"") > 0"
    PRINT "instr= "; AI.STATELOAD("wev2", "verified")

    ' ── 3) confronto numerico e residuo portabile di controllo ────────────────────────────
    AI WORKER NEW "wev3", "{""workspace"":""work/wev3"",""budget"":4000}"
    AI WORKER LOOP "VERIFY", 2 REQUIRE "LEN(RESULT$) > 5"
    PRINT "len5= "; AI.STATELOAD("wev3", "verified")
END SUB
